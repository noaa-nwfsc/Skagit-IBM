#!/bin/bash
## Job Name
#SBATCH --job-name=chinook_test_9thread
## Allocation Definition
#SBATCH --account=stf
#SBATCH --partition=stf
## Resources
## Nodes
#SBATCH --nodes=1      
## Tasks per node (Slurm assumes you want to run 28 tasks, remove 2x # and adjust parameter if needed)
#SBATCH --ntasks-per-node=3
## Set number of cores per task to allow multithreading
#SBATCH --cpus-per-task=9
## Walltime (two hours)
#SBATCH --time=10:00:00
# E-mail Notification, see man sbatch for options
 

##turn on e-mail notification

#SBATCH --mail-type=ALL

#SBATCH --mail-user=aebratt@uw.edu


## Memory per node
#SBATCH --mem=100G
## Specify the working directory for this job
#SBATCH --chdir=/gscratch/stf/aebratt/fish_cpp_hyak

module load parallel-20170722

seq 6 | parallel -j 3 -n0 "/gscratch/stf/aebratt/fish_cpp_hyak/bin/headless test_run_listings.csv output config_diana.json"
