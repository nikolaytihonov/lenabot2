CC=g++
AR=ar
OBJ=json.o
OBJ_DIR=obj/
OUT=libjson.a

%.o: %.cpp
	$(CC) -c $< -o $(OBJ_DIR)$@

$(OUT): $(OBJ)
	$(AR) rcs $(OUT) $(addprefix $(OBJ_DIR),$(OBJ))
