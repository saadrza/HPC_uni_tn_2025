#!/bin/bash
#PBS -N matmul_openmp_job
#PBS -l select=1:ncpus=32:mem=2gb -l place=pack:excl
#PBS -l walltime=5:00:00
#PBS -q short_cpuQ

# ---- Configurable section ----
src_file="matmul_openmpi.c"
dim=16384
cores=32
threads=2
# Extract base name without extension
base_name=$(basename "$src_file" .c)

# Output log files
#PBS -o ${base_name}_${dim}_${cores}_output.out
#PBS -e ${base_name}_${dim}_${cores}_error.log

# ---- Load modules ----
module load mpich-3.2
#module load openmpi

# ---- Paths ----
project_root="/home/saad.hussainshafi/HPC_uni_tn_2025/HPC_uni_tn_2025/parallel_hybrid"
bin_dir="$project_root/bin/openmpi/$dim"
log_dir="$project_root/logs/openmpi/$dim"
src_dir="$project_root/src/main"

mkdir -p "$bin_dir"
mkdir -p "$log_dir"

export OMP_NUM_THREADS=$threads
echo "Using OMP_NUM_THREADS=$OMP_NUM_THREADS"

# ---- Compilation ----
cd "$src_dir"
exe_file="$bin_dir/${base_name}_${dim}"

mpicc -O3 -fopenmp -std=c99 -DMATRIX_SIZE=$dim "$src_dir/$src_file" -o "$exe_file" -lm
if [ ! -f "$exe_file" ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi

# ---- Execution ----
"$exe_file" "$dim" > "$log_dir/${base_name}_${dim}_${cores}_output.log" \
                   2> "$log_dir/${base_name}_${dim}_${cores}_error.log"
