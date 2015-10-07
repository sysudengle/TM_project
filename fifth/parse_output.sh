
init=$1

for i in 10 20 40 60 80; do
	for j in 15 30 50 100; do
		file=${1}_${j}_${i}.txt
		#echo $file
		value=$(head -25 $file |tail -1|awk -F','  '{print $2}')
		val="76"
		val=$value
		val=($val/50000000)*100
		echo $value
	done
done
