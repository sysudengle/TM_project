
init=$1

for i in 10 20 40 60 80; do
	for j in 15 30 50 100; do
		file=${1}_${j}_${i}.txt
		#echo $file
		./array_tm 4 $1 $j 4 0 $i 0 > $file &
	done
done
