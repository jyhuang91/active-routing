bench=lud
folder=dfly_log_runtimeknob_${bench}

mkdir -p ${folder}
# benchmarks
for option in 0_2048 1_2048 2_2048
do
    ${PWD}/mcsim -mdfile ../Apps/md/md-16o3core4GHz-mesh-hmc.py -runfile ../Apps/list/run-rb-${bench}_${option}.py -benchname dfly_arftid_${bench}_${option} > ${folder}/arftid_${bench}_${option}.log 2>&1 &
done
