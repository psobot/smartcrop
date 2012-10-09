default:
	g++ -ljpeg -O3 smartcrop.cpp -o smartcrop

debug:
	g++ -ggdb -ljpeg -O3 smartcrop.cpp -o smartcrop

clean:
	rm smartcrop
	rm -rf smartcrop.dSYM
