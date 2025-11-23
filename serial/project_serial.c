/* =========================
   PD project - serial
   ========================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

/* ---------- Constants ---------- */
// Fixed numbers stored in one place
#define MAX_LINE (1<<20)           // buffer for reading a line
#define BURST_WINDOW_SEC 300       // 5 minutes
#define BURST_COUNT_THRESHOLD 3    // 3 or more operations in 5 minutes considered suspicious

/* ---------- Struct ---------- */
// Holds the columns of a CSV row in one place
typedef struct {
    long long cc_num;
    long long unix_time;
    char *category;
    int is_fraud;
} Transaction;

/* ---------- Split CSV line using strtok ---------- */
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

/* ---------- Convert string to long long ---------- */
static long long to_ll(const char *s) {
    while (*s == '"' || *s == ' ' || *s == '\t') {
        s++;
    }
    return strtoll(s, NULL, 10);
}

/* ---------- Comparison function for sorting ---------- */
static int cmp_tx(const void *A, const void *B) {
    const Transaction *a = (const Transaction *)A;
    const Transaction *b = (const Transaction *)B;

    int cmp_cc;
    if (a->cc_num > b->cc_num)
        cmp_cc = 1;
    else if (a->cc_num < b->cc_num)
        cmp_cc = -1;
    else
        cmp_cc = 0;

    // If credit cards are different, return that comparison
    if (cmp_cc != 0)
        return cmp_cc;

    if (a->unix_time > b->unix_time)
        return 1;
    else if (a->unix_time < b->unix_time)
        return -1;
    else
        return 0;
}

/* ---------- Feature 1: Burst Detection ---------- */
size_t countBurst(const Transaction a[], size_t n, long long W, size_t T) {
    size_t suspicious = 0;

    for (size_t i = 0; i < n; ) {
        long long card = a[i].cc_num;
        size_t j = i;
        while (j < n && a[j].cc_num == card) j++;

        size_t start = i, end = i;
        while (start < j) {
            while (end < j && (a[end].unix_time - a[start].unix_time) <= W)
                end++;

            if ((end - start) >= T)
                suspicious++;

            start++;
        }
        i = j;
    }

    return suspicious;
}

/* ---------- Feature 2: Category Novelty ---------- */
int cmp_str(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

size_t countCategoryNovelty_likeSource(const Transaction a[], size_t n) {
    size_t suspicious = 0;

    for (size_t i = 0; i < n; ) {
        long long card = a[i].cc_num;
        size_t j = i;
        while (j < n && a[j].cc_num == card) j++;

        size_t m = j - i;
        char **cats = malloc(m * sizeof(char*));
        for (size_t k = 0; k < m; k++)
            cats[k] = a[i + k].category;

        qsort(cats, m, sizeof(char*), cmp_str);

        size_t unique = 0;
        for (size_t k = 0; k < m; k++) {
            if (k == 0 || strcmp(cats[k], cats[k - 1]) != 0)
                unique++;
        }

        suspicious += unique;
        free(cats);
        i = j;
    }

    return suspicious;
}

/* ---------- Main Function ---------- */
int main(void) {
    const char *path = "../dataset/data.csv";

    // Open file and read columns
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Error opening %s\n", path);
        return 1;
    }

    /* Read header */
    char *line = (char*)malloc(MAX_LINE);
    if (!line) { fclose(f); return 1; }
    fgets(line, MAX_LINE, f);

    /* Load data into dynamic array */
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
            arr = (Transaction*)tmp;
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

    /* Sort data before feature calculation */
    qsort(arr, n, sizeof(Transaction), cmp_tx);

    /* Compute features */
    double t0 = omp_get_wtime();   

    size_t suspicious_burst = countBurst(arr, n, (long long)BURST_WINDOW_SEC, (size_t)BURST_COUNT_THRESHOLD);
    size_t suspicious_cat   = countCategoryNovelty_likeSource(arr, n);

    double t1 = omp_get_wtime(); 

    /* Print results */
    printf("Rows read: %zu\n", n);
    printf("Suspicious (Transaction Frequency): %zu\n", suspicious_burst);
    printf("Suspicious (Unusual Categories):    %zu\n", suspicious_cat);
    printf("Serial elapsed: %.3f s\n", t1 - t0);

    /* Free memory */
    for (size_t i = 0; i < n; ++i) free(arr[i].category);
    free(arr);

    return 0;
}
