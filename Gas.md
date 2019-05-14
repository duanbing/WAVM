## Motivation

Metering the resoure cost of a wasm module instruction by instruction. The main idea is adding a metering function call
at the beginning of each [basic block](https://en.wikipedia.org/wiki/Basic_block) as below:

```
i64.const ${gas_counter}
call $add_gas_func
```

And `add_gas_func`` accepts one parameter `gas_counter` to count all the instruction's gas cost in currerent basic block, 
which is declared as below:
```
type (;1;) (func (param i32 i32))
(import "env" "_add_gas" (func (;0;) (type 1)))
```
in wast file.

Then, walk all the branch instructions ([block, if, else, loop, br, br_if, br_table, loop, return, end]) per defined funtion in wasm module,
and insert metering instructions behind.

## Implementation

Firstly we should figure out cycle costed by a instruction, and fill them in a gas cost table.
Gas cost table had been referred to [ewasm project](https://github.com/ewasm/design/blob/master/determining_wasm_gas_costs.md), but swappable. 
we also can use [llvm::getInstructionCost](https://reviews.llvm.org/D37170?id=113616) to re-evaluate the cycles costed.

Cost table supports MVP only now.


This whole process run as below:

```
insert add_gas function -> compile c/c++ to wasm -> insert metering instructions -> set gas limit -> run wasm module 
```

* insert add_gas function:  `add_gas` imported function should not be accessed by module developer, so to avoid adjust the index space of import section, Inserting a imported function need update the function index space, which mean:
those segment as below should be updated.
```
- IR::Module::exports
- IR::Module::elemSegments
- IR::Module::startFunctionIndex
- IR::FunctionImm in the byte code for call instructions
```
the implementation code is in `Programs/wavm-as/insert-imported-context.h`.

* compile c/c++ to wasm:  we can use [emscripten](https://emscripten.org/docs/introducing_emscripten/index.html) to compile the c/c++ source file to wasm format. 

For example:
```
emcc gas.cpp -Oz -s EXPORTED_FUNCTIONS='["_main","_add"]' -o gas.js
```
`Oz` option will do full optimization, so the memory section probably is omitted, so transfer wasm to wast and add a memory section.

* insert metering instructions : insert add_gas call after branch instruction. call GasVisitor::addGas. code is [here](https://github.com/duanbing/WAVM/blob/master/Programs/wavm-run/GasVisitContext.h)

* set gas limit: call Emscripten::setGasLimit before invokeFunction. 

* run the module:  

```
./bin/wavm-run -d ../Examples/gas.wast
```
* get gas used : call Emscripten::getGasUsed after invokeFunction.

