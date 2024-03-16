gcc -o spx_exchange spx_exchange.c
gcc -o test_trader test_trader.c

cp tests/EndToEndTests/1trader_all_commands.in tests/EndToEndTests/current_test/test_0.in
cp tests/EndToEndTests/1trader_all_commands.out tests/EndToEndTests/current_test/test_0.out
echo
echo Running test 1:
./spx_exchange products.txt ./test_trader > exchange.out
diff tests/EndToEndTests/current_test/temp_0.out tests/EndToEndTests/current_test/test_0.out
echo Test 1... ok
rm tests/EndToEndTests/current_test/temp_0.out

cp tests/EndToEndTests/2traders_complex_matches_t0.in tests/EndToEndTests/current_test/test_0.in
cp tests/EndToEndTests/2traders_complex_matches_t0.out tests/EndToEndTests/current_test/test_0.out

cp tests/EndToEndTests/2traders_complex_matches_t1.in tests/EndToEndTests/current_test/test_1.in
cp tests/EndToEndTests/2traders_complex_matches_t1.out tests/EndToEndTests/current_test/test_1.out
echo
echo Running test 2:
./spx_exchange products.txt ./test_trader ./test_trader > exchange.out
diff tests/EndToEndTests/current_test/temp_0.out tests/EndToEndTests/current_test/test_0.out
diff tests/EndToEndTests/current_test/temp_1.out tests/EndToEndTests/current_test/test_1.out
echo Test 2... ok
rm tests/EndToEndTests/current_test/temp_0.out
rm tests/EndToEndTests/current_test/temp_1.out

cp tests/EndToEndTests/2traders_simple_matches_t0.in tests/EndToEndTests/current_test/test_0.in
cp tests/EndToEndTests/2traders_simple_matches_t0.out tests/EndToEndTests/current_test/test_0.out

cp tests/EndToEndTests/2traders_simple_matches_t1.in tests/EndToEndTests/current_test/test_1.in
cp tests/EndToEndTests/2traders_simple_matches_t1.out tests/EndToEndTests/current_test/test_1.out
echo
echo Running test 3:
./spx_exchange products.txt ./test_trader ./test_trader > exchange.out
diff tests/EndToEndTests/current_test/temp_0.out tests/EndToEndTests/current_test/test_0.out
diff tests/EndToEndTests/current_test/temp_1.out tests/EndToEndTests/current_test/test_1.out
echo Test 3... ok
rm tests/EndToEndTests/current_test/temp_0.out
rm tests/EndToEndTests/current_test/temp_1.out

