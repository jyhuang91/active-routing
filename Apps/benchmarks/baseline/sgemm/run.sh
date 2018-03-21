# small
#./sgemm -i ./datasets/small/input/matrix1.txt,./datasets/small/input/matrix2.txt,./datasets/small/input/matrix2t.txt -o ./output/small/matrix3.txt -n 8
#./sgemm -i 128,96,160 -o ./output/small/matrix3.txt -n 8

# medium
#./sgemm -i datasets/medium/input/matrix1.txt,datasets/medium/input/matrix2t.txt,datasets/medium/input/matrix2t.txt -o output/medium/matrix3.txt -n 8
#./sgemm -i datasets/medium/input/matrix1.txt,./datasets/medium/input/matrix2t.txt,./datasets/medium/input/matrix2t.txt -n 8
#./sgemm -i 1024,992,1056 -o ./output/medium/matrix3.txt -n 8

# self defined input
./sgemm -i 512,256,512 -n 8
