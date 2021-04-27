#!/usr/bin/env bash

if [ ! -d "files_output" ]; then
    mkdir files_output
fi
if [ ! -d "files_output/results" ]; then
    mkdir files_output/results
fi

for sve in 0 1 ; do
    make clean
    make compile_gem5 SVE=$sve
   
    hotspot_iter=1 # iterations in the program hospot_gem5
    iter=100 # iterations for this script
    vl=8  # vector length for which the code was optimized

    for cpu in ex5_LITTLE ex5_big ; do
        for size in 64 128 256 ; do
            touch temp.out
            printf "\n\n"
            for ((i=1;i<=iter;i++)) ; do
                if [ $sve == 1 ] ; then printf "sve: $sve; cpu: $cpu; vl: $vl; size: $size \t i: $i\n"
                else printf "sve: $sve; cpu: $cpu; size: $size \t i: $i\n"
                fi
                make run_gem5 S=$size I=$hotspot_iter CPU=$cpu VL=$vl 2>/dev/null >> temp.out
            done

            printf "SVE: $sve \t CPU: $cpu \t Size: $size\n\n" > files_output/results/SVE${sve}_CPU${cpu}_S${size}.out

            for keyword in loop Transient ; do
                cat temp.out | grep $keyword | cut -d':' -f2 | cut -d' ' -f2 > proc.out

                arr=($(cat proc.out | sort -n))
                if (( $iter % 2 == 1 )); then     # Odd number of elements
                    median=$(echo "${arr[ $(($iter/2)) ]}" | xargs printf %.3f)
                else                             # Even number of elements
                    (( j=iter/2 ))
                    (( k=j-1 ))
                fi
                median=$(echo "(${arr[j]} + ${arr[k]})/2" | bc -l | xargs printf %.3f)
                
                min=$(echo "${arr[0]}" | xargs printf %.3f)
                max=$(echo "${arr[iter-1]}" | xargs printf %.3f)
                
                total=0; count=0;
                for i in $( awk '{ print $1; }' proc.out ) ; do
                    total=$(echo "$total+$i" | bc -l | xargs printf %.3f )
                    ((count++))
                done
                avg=$(echo "$total/$count" | bc -l | xargs printf %.3f)

                if [ $keyword == loop ] ; then 
                     printf "\tfraction:\n" >> files_output/results/SVE${sve}_CPU${cpu}_S${size}.out
                else printf "\tfunction:\n" >> files_output/results/SVE${sve}_CPU${cpu}_S${size}.out
                fi
                printf "\t\tmin: $min us; \tavg: $avg us; \tmedian: $median us; \tmax: $max us\n\n" \
                    >> files_output/results/SVE${sve}_CPU${cpu}_S${size}.out
            done    
            rm temp.out proc.out
        done
    done
done
