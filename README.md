##

~~1. add and sub support~~  
~~2. atoi and python support~~  
~~3. cpp and python test cases~~  
~~1. mul and div support~~  
~~2. fine sub and support negative (with + - flag)~~  
~~1. refine mulT and mulSubT~~  
~~3. impl += -= *= /=~~  
~~2. impl mul with negative~~  

2. impl div
    0x03FF / 2 = 0x01FF
    [03, FF] -> [00, 03, 3F, FF]

3. impl mod with negative
4. impl mod, mod_inverse, pow, [q, r = (a // b, x % y)]
5. remove some virtual function
6. impl wrapper for python and test


Note:
1. uint128_t += uint128_t, uint128_t -= uint128_t
2. uint128_t = uint128_t +/- uint128_t
3. uint128 *= uint128
4. uint128 = uint128 * uint128


https://blog.quarkslab.com/turning-regular-code-into-atrocities-with-llvm.html
https://github.com/quarkslab/llvm-passes/tree/master/llvm-passes

https://people.eecs.berkeley.edu/~fateman/282/F%20Wright%20notes/week5.pdf
https://www.researchgate.net/publication/221588714_Practical_Divide-and-Conquer_Algorithms_for_Polynomial_Arithmetic
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.47.565&rep=rep1&type=pdf
https://gmplib.org/manual/Divide-and-Conquer-Division.html
