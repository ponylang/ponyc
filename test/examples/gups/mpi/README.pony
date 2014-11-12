/usr/local/Cellar/open-mpi16/1.6.5/bin/mpicc -O3 -march=native -Wa,-q -DLONG64 -o gups_vanilla gups_vanilla.c

/usr/local/Cellar/open-mpi16/1.6.5/bin/mpicc -O3 -march=native -Wa,-q -DLONG64 -o gups_opt gups_opt.c

/usr/local/Cellar/open-mpi16/1.6.5/bin/mpirun -np 4 gups_opt 20 1000 1024

