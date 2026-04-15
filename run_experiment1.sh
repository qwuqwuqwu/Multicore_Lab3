#!/bin/bash
#
# run_experiment1.sh
# Experiment 1: Speedup over single-thread version
#
# City sizes : 4, 8, 12
# Thread counts: 1, 2, 3, 4
# Trials per scenario: 5
# Output: experiment1_results.csv

CITIES_LIST=(4 8 12)
THREAD_LIST=(1 2 3 4)
TRIALS=5
CSV="experiment1_results.csv"

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

    # tsm writes citiesN.txt to disk and prints something to stdout
    echo "  Generating cities${n}.txt ..."
    ./tsm "$n" > /dev/null

    if [ ! -f "cities${n}.txt" ]; then
        echo "  ERROR: cities${n}.txt not created by tsm. Aborting."
        exit 1
    fi

    # tsmoptimal prints "total weight = X"
    echo "  Running tsmoptimal ..."
    opt_output=$(./tsmoptimal "$n" "cities${n}.txt" 2>&1)
    echo "$opt_output" | sed 's/^/    /'

    # Extract the numeric weight from the last line that contains "total weight"
    opt_dist=$(echo "$opt_output" | grep "total weight" | awk '{print $NF}')
    OPTIMAL[$n]="$opt_dist"
    echo "  => Optimal distance for $n cities: ${OPTIMAL[$n]}"
    echo ""
done

# ── Helper: convert bash time string "XmY.ZZZs" to decimal seconds ───────────
parse_real_time() {
    # Input example: "0m0.045s"
    local raw="$1"
    local minutes=$(echo "$raw" | sed 's/m.*//')
    local seconds=$(echo "$raw" | sed 's/.*m//' | sed 's/s//')
    awk "BEGIN { printf \"%.3f\", $minutes * 60 + $seconds }"
}

# ── 3 & 4 & 5. Run ptsm, measure time, 5 trials ──────────────────────────────
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
            # Run ptsm under bash's time builtin; capture stderr (where time writes)
            time_output=$( { time ./ptsm "$n" "$t" "cities${n}.txt" ; } 2>&1 >/dev/null )

            # Extract the "real" line, e.g. "real   0m0.012s"
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
