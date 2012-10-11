COMPILER=clang++
FLAGS=

OUTPUT1=smartcrop
SRC1=smartcrop.cpp timeme.cpp
INC1=`Magick++-config --cppflags --cxxflags --ldflags --libs`

OUTPUT2=fastcrop
SRC2=fastcrop.cpp
INC2=-ljpeg

default:
	${COMPILER} ${FLAGS} ${INC1} -O3 ${SRC1} -o ${OUTPUT1}
	${COMPILER} ${FLAGS} ${INC2} -O3 ${SRC2} -o ${OUTPUT2}

debug:
	${COMPILER} ${FLAGS} -ggdb -D DEBUG ${INC1} ${SRC1} -o ${OUTPUT1}
	${COMPILER} ${FLAGS} -ggdb -D DEBUG ${INC2} ${SRC2} -o ${OUTPUT2}

clean:
	rm ${OUTPUT1}
	rm -rf ${OUTPUT1}.dSYM
	rm ${OUTPUT2}
	rm -rf ${OUTPUT2}.dSYM
