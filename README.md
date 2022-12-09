# psdd

1. clone this repo
5. from psdd folder run `cmake .`
6. run `make`
9. run `g++ -no-pie -o mult_sdd2psdd mult_sdd2psdd.cpp src/psdd_manager.cpp src/psdd_node.cpp src/psdd_parameter.cpp src/psdd_unique_table.cpp src/random_double_generator.cpp -Iinclude -Llib/linux -lsdd -lgmp`
1. now you can multiply sdd files into psdds running `./mult_sdd2psdd file.vtree file1.sdd file2.sdd ... filen.sdd resultfile.psdd`