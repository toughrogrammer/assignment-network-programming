PATH_BUILD = ./build
OBJS = $(PATH_BUILD)/main.o

main: main.o
	@- mkdir -p $(PATH_BUILD);
	@- echo "build::"
	gcc -lm -o $(PATH_BUILD)/$@ $(OBJS)
	@- echo ""

%.o: %.c
	@- mkdir -p $(PATH_BUILD);
	gcc -g -c -o $(PATH_BUILD)/$@ $<

clean:
	@- echo "cleaning::"
	rm -rf $(PATH_BUILD)/*
	rm -rf ./test
	rm -rf ./test2
	@- echo ""

run:
	@- echo "result of run::"
	@- $(PATH_BUILD)/main $(port)
	@- echo ""

clean-run:
	make clean
	make
	make run
