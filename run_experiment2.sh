#!/bin/bash
#
# run_experiment2.sh
# Experiment 2: Speedup vs number of cities (fixed 4 threads)
#
# City sizes : 6, 8, 10, 12  (increment of 2)
# Thread counts: 1 (baseline), 4 (fixed)
# Trials per scenario: 5
# Output: experiment2_results.csv

CITIES_LIST=(6 8 10 12)
THREAD_LIST=(1 4)
TRIALS=5
CSV="experiment2_results.csv"

# ── 1. Compile ────────────────────────────────────────────────────────────────
echo "========================================"
echo " Step 1: Compiling ptsm.c"
echo "========================================"
gcc -O2 -fopenmp -o ptsm ptsm.c -lm
if [ $? -ne 0 ]; then
    echo "ERROR: Compilation failed. Aborting."
    exit 1
fi
echo "Compilation successful."
echo ""

# ── 2. Generate city files + get optimal distances ────────────────────────────
echo "========================================"
echo " Step 2: Generating city files and optimal distances"
echo "========================================"

declare -A OPTIMAL   # OPTIMAL[n] = optimal distance for n cities

for n in "${CITIES_LIST[@]}"; do
    echo "--- $n cities ---"

    # Skip generation if file already exists (e.g. cities8.txt from experiment 1)
    if [ ! -f "cities${n}.txt" ]; then
        echo "  Generating cities${n}.txt ..."
        ./tsm "$n" > /dev/null
    else
        echo "  cities${n}.txt already exists, skipping generation."
    fi

    if [ ! -f "cities${n}.txt" ]; then
        echo "  ERROR: cities${n}.txt not created by tsm. Aborting."
        exit 1
    fi

    echo "  Running tsmoptimal ..."
    opt_output=$(./tsmoptimal "$n" "cities${n}.txt" 2>&1)
    echo "$opt_output" | sed 's/^/    /'

    opt_dist=$(echo "$opt_output" | grep "total weight" | awk '{print $NF}')
    OPTIMAL[$n]="$opt_dist"
    echo "  => Optimal distance for $n cities: ${OPTIMAL[$n]}"
    echo ""
done

# ── Helper: convert bash time string "XmY.ZZZs" to decimal seconds ───────────
parse_real_time() {
    local raw="$1"
    local minutes=$(echo "$raw" | sed 's/m.*//')
    local seconds=$(echo "$raw" | sed 's/.*m//' | sed 's/s//')
    awk "BEGIN { printf \"%.3f\", $minutes * 60 + $seconds }"
}

# ── 3. Run ptsm, measure time, 5 trials ──────────────────────────────────────
echo "========================================"
echo " Step 3: Running ptsm experiments"
echo "========================================"

# CSV header
echo "cities,threads,optimal,trial_1,trial_2,trial_3,trial_4,trial_5" > "$CSV"

for n in "${CITIES_LIST[@]}"; do
    for t in "${THREAD_LIST[@]}"; do
        row="${n},${t},${OPTIMAL[$n]}"
        echo -n "  cities=$n  threads=$t  | "

        for trial in $(seq 1 $TRIALS); do
            time_output=$( { time ./ptsm "$n" "$t" "cities${n}.txt" ; } 2>&1 >/dev/null )
            real_raw=$(echo "$time_output" | grep "^real" | awk '{print $2}')
            real_sec=$(parse_real_time "$real_raw")

            row="${row},${real_sec}"
            echo -n "t${trial}=${real_sec}s "
        done

        echo ""
        echo "$row" >> "$CSV"
    done
    echo ""
done

# ── Done ──────────────────────────────────────────────────────────────────────
echo "========================================"
echo " Done! Results written to $CSV"
echo "========================================"
echo ""
cat "$CSV"
