#!/bin/bash

# Executable path
EXE="./Project_parallel.exe"
DATA="../dataset/data.csv"

# Different number of threads
THREADS=(1 2 4 8)
# OpenMP scheduling types
SCHEDULES=("static" "dynamic" "guided")
# Chunk sizes
CHUNKS=(1 10 100 1000)
# Number of repetitions per experiment
REPEATS=3

# CSV file to store results
OUTPUT="parallel_results.csv"
echo "Threads,Schedule,Chunk,MeanTime,StdDev,Speedup,Efficiency,Mean_Suspicious_Burst,Mean_Suspicious_Category" > $OUTPUT

# First: determine baseline time for 1 thread, static schedule, chunk=1
BASELINE=0
times=()
for ((r=1;r<=REPEATS;r++)); do
    output=$($EXE $DATA 1 static 1)
    t=$(echo "$output" | grep "Parallel elapsed" | awk '{print $3}')
    times+=($t)
done

# Calculate mean baseline time
sum=0
for t in "${times[@]}"; do sum=$(echo $sum + $t | bc -l); done
BASELINE=$(echo "$sum / $REPEATS" | bc -l)

# Function to calculate standard deviation
function stddev {
    arr=("$@")
    n=${#arr[@]}
    sum=0
    for x in "${arr[@]}"; do sum=$(echo $sum + $x | bc -l); done
    mean=$(echo "$sum / $n" | bc -l)
    sqsum=0
    for x in "${arr[@]}"; do 
        diff=$(echo "$x - $mean" | bc -l)
        sq=$(echo "$diff * $diff" | bc -l)
        sqsum=$(echo "$sqsum + $sq" | bc -l)
    done
    std=$(echo "sqrt($sqsum / $n)" | bc -l)
    echo $std
}

# Loop over all experiment configurations
for th in "${THREADS[@]}"; do
    for sch in "${SCHEDULES[@]}"; do
        for ch in "${CHUNKS[@]}"; do
            times=()
            burst_vals=()
            cat_vals=()
            echo "Running: Threads=$th Schedule=$sch Chunk=$ch"
            for ((r=1;r<=REPEATS;r++)); do
                output=$($EXE $DATA $th $sch $ch)

                # Execution time
                t=$(echo "$output" | grep "Parallel elapsed" | awk '{print $3}')
                times+=($t)

                # Suspicious transaction metrics
                suspicious_burst=$(echo "$output" | grep "Suspicious (Transaction Frequency)" | awk -F':' '{gsub(/ /,"",$2); print $2}')
                suspicious_cat=$(echo "$output" | grep "Suspicious (Unusual Categories)" | awk -F':' '{gsub(/ /,"",$2); print $2}')
                burst_vals+=($suspicious_burst)
                cat_vals+=($suspicious_cat)
            done

            # Compute mean and standard deviation of execution time
            sum=0
            for t in "${times[@]}"; do sum=$(echo $sum + $t | bc -l); done
            mean=$(echo "$sum / $REPEATS" | bc -l)
            std=$(stddev "${times[@]}")

            # Compute mean of suspicious metrics
            sum_burst=0
            for b in "${burst_vals[@]}"; do sum_burst=$(echo $sum_burst + $b | bc -l); done
            mean_burst=$(echo "$sum_burst / $REPEATS" | bc -l)

            sum_cat=0
            for c in "${cat_vals[@]}"; do sum_cat=$(echo $sum_cat + $c | bc -l); done
            mean_cat=$(echo "$sum_cat / $REPEATS" | bc -l)

            # Calculate speedup and efficiency
            speedup=$(echo "$BASELINE / $mean" | bc -l)
            efficiency=$(echo "$speedup / $th" | bc -l)

            # Save results to CSV
            echo "$th,$sch,$ch,$mean,$std,$speedup,$efficiency,$mean_burst,$mean_cat" >> $OUTPUT
        done
    done
done

echo "All experiments completed. Results saved in $OUTPUT"
