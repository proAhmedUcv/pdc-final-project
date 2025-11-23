/* =========================
   PD project - parallel (OpenMP, tunable)
   ========================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

/* ---------- Constants ---------- */
#define MAX_LINE (1<<20)
#define BURST_WINDOW_SEC 300
#define BURST_COUNT_THRESHOLD 3

/* ---------- Transaction Struct ---------- */
typedef struct {
    long long cc_num;
    long long unix_time;
    char *category;
    int is_fraud;
} Transaction;

/* ---------- CSV Split ---------- */
static int split_csv(char *line, char **fields, int max_fields) {
    int count = 0;
    char *token;
    for (token = strtok(line, ",\r\n");
         token && count < max_fields;
         token = strtok(NULL, ",\r\n")) {
        fields[count++] = token;
    }
    return count;
}

/* ---------- String to long long ---------- */
static long long to_ll(const char *s) {
    while (*s == '"' || *s == ' ' || *s == '\t') s++;
    return strtoll(s, NULL, 10);
}

/* ---------- Transaction Comparison for qsort ---------- */
static int cmp_tx(const void *A, const void *B) {
    const Transaction *a = (const Transaction *)A;
    const Transaction *b = (const Transaction *)B;

    int cmp_cc = (a->cc_num > b->cc_num) - (a->cc_num < b->cc_num);
    if (cmp_cc != 0) return cmp_cc;
    return (a->unix_time > b->unix_time) - (a->unix_time < b->unix_time);
}

/* ---------- Build card blocks ---------- */
static void build_card_blocks(const Transaction *a, size_t n,
                              size_t **out_starts, size_t **out_ends, size_t *out_nb)
{
    if (n == 0) {
        *out_starts = NULL;
        *out_ends   = NULL;
        *out_nb     = 0;
        return;
    }

    size_t nb = 1;
    for (size_t i = 1; i < n; ++i)
        if (a[i].cc_num != a[i - 1].cc_num) ++nb;

    size_t *starts = malloc(nb * sizeof *starts);
    size_t *ends   = malloc(nb * sizeof *ends);

    size_t bi = 0, s = 0;
    for (size_t i = 1; i <= n; ++i) {
        if (i == n || a[i].cc_num != a[i - 1].cc_num) {
            starts[bi] = s;
            ends[bi]   = i;
            ++bi;
            s = i;
        }
    }

    *out_starts = starts;
    *out_ends   = ends;
    *out_nb     = nb;
}

/* ---------- Burst Count Parallel ---------- */
static size_t countBurst_parallel(const Transaction a[], size_t n,
                                  long long W, size_t T,
                                  const size_t *starts, const size_t *ends, size_t nb)
{
    size_t sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (size_t bi = 0; bi < nb; ++bi) {
        size_t i = starts[bi], j = ends[bi];
        size_t start = i, end = i;
        size_t local_cnt = 0;
        while (start < j) {
            while (end < j && (a[end].unix_time - a[start].unix_time) <= W) end++;
            if ((end - start) >= T) local_cnt++;
            start++;
        }
        sum += local_cnt;
    }
    return sum;
}

/* ---------- Category Novelty Parallel ---------- */
int cmp_str(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

static size_t countCategoryNovelty_parallel(const Transaction a[], size_t n,
                                            const size_t *starts, const size_t *ends, size_t nb)
{
    size_t sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (size_t bi = 0; bi < nb; ++bi) {
        size_t i = starts[bi], j = ends[bi];
        size_t m = j - i;
        if (m == 0) continue;

        char **cats = malloc(m * sizeof *cats);
        if (!cats) continue;

        for (size_t k = 0; k < m; k++)
            cats[k] = a[i + k].category;

        qsort(cats, m, sizeof *cats, cmp_str);

        size_t unique = 0;
        for (size_t k = 0; k < m; ) {
            unique++;
            size_t t = k + 1;
            while (t < m && strcmp(cats[t], cats[k]) == 0) t++;
            k = t;
        }

        sum += unique;
        free(cats);
    }
    return sum;
}

/* ---------- Main ---------- */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <dataset_path> [threads] [schedule] [chunk_size]\n", argv[0]);
        printf("Example: %s ../dataset/data.csv 8 dynamic 100\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    int num_threads = (argc >= 3) ? atoi(argv[2]) : omp_get_max_threads();
    omp_set_num_threads(num_threads);

    omp_sched_t schedule_type = omp_sched_static;
    int chunk_size = 0;
    if (argc >= 4) {
        if (strcmp(argv[3], "dynamic") == 0) schedule_type = omp_sched_dynamic;
        else if (strcmp(argv[3], "guided") == 0) schedule_type = omp_sched_guided;
        else schedule_type = omp_sched_static;
    }
    if (argc >= 5) chunk_size = atoi(argv[4]);
    omp_set_schedule(schedule_type, chunk_size);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Error opening %s\n", path);
        return 1;
    }

    char *line = malloc(MAX_LINE);
    if (!line) { fclose(f); return 1; }
    fgets(line, MAX_LINE, f);  // skip header

    Transaction *arr = NULL;
    size_t n = 0, cap = 0;
    char *fields[4];

    while (fgets(line, MAX_LINE, f)) {
        int m = split_csv(line, fields, 4);
        if (m < 3) continue;

        if (n == cap) {
            size_t new_cap = cap ? cap * 2 : 1024;
            void *tmp = realloc(arr, new_cap * sizeof *arr);
            if (!tmp) { free(line); fclose(f); free(arr); return 1; }
            arr = tmp;
            cap = new_cap;
        }

        arr[n].cc_num    = to_ll(fields[0]);
        arr[n].category  = strdup(fields[1]);
        arr[n].unix_time = to_ll(fields[2]);
        arr[n].is_fraud  = (m >= 4 && fields[3][0] == '1') ? 1 : 0;
        n++;
    }
    fclose(f);
    free(line);

    qsort(arr, n, sizeof(Transaction), cmp_tx);

    size_t *starts = NULL, *ends = NULL, nb = 0;
    build_card_blocks(arr, n, &starts, &ends, &nb);

    double t0 = omp_get_wtime();
    size_t suspicious_burst = countBurst_parallel(arr, n, BURST_WINDOW_SEC,
                                                   BURST_COUNT_THRESHOLD,
                                                   starts, ends, nb);
    size_t suspicious_cat = countCategoryNovelty_parallel(arr, n,
                                                          starts, ends, nb);
    double t1 = omp_get_wtime();
    double elapsed = t1 - t0;

    // --------- Print results to screen ----------
    printf("Rows read: %zu\n", n);
    printf("Threads used: %d\n", omp_get_max_threads());
    printf("Schedule: %d, Chunk size: %d\n", schedule_type, chunk_size);
    printf("Suspicious (Transaction Frequency): %zu\n", suspicious_burst);
    printf("Suspicious (Unusual Categories):    %zu\n", suspicious_cat);
    printf("Parallel elapsed: %.3f s\n", elapsed);

    // --------- Write results to CSV ----------
    FILE *out;
    if (access("parallel_results.csv", F_OK) != 0) {
        // File doesn't exist, write header
        out = fopen("parallel_results.csv", "w");
        fprintf(out, "Threads,Schedule,Chunk,ExecutionTime,Suspicious_Burst,Suspicious_Category\n");
        fclose(out);
    }

    // Append current run
    out = fopen("parallel_results.csv", "a");
    if (out) {
        fprintf(out, "%d,%d,%d,%.6f,%zu,%zu\n",
                num_threads, schedule_type, chunk_size, elapsed,
                suspicious_burst, suspicious_cat);
        fclose(out);
    }

    // --------- Free memory ----------
    for (size_t i = 0; i < n; ++i) free(arr[i].category);
    free(arr);
    free(starts);
    free(ends);

    return 0;
}

