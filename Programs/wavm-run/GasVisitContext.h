#pragma once
#include <stdint.h>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "WAVM/IR/Module.h"
#include "WAVM/IR/OperatorPrinter.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Logging/Logging.h"

using namespace WAVM;
using namespace WAVM::IR;

typedef CodeValidationProxyStream<OperatorEncoderStream> CodeStream;
typedef void OperatorEmitFunc(CodeStream*);

struct GasVisitor {
    typedef I64 Result;
    GasVisitor(Uptr idx, IR::Module& irModule, IR::FunctionDef& fd)
        : gasCounter(0), addGasFuncIndex(idx), module(irModule), functionDef(fd) {}

    ~GasVisitor() { delete encoderStream; encoderStream = nullptr; }

    CodeStream *encoderStream;
    I64 gasCounter;  //trace gas used by current block
    Uptr addGasFuncIndex; //gas stat function index
    IR::Module& module;
    IR::FunctionDef& functionDef;

    std::vector<std::function<OperatorEmitFunc>> opEmiters;

    void put_trap()
    {
        //printf("gas=%d, size=%lu\n",gasCounter, opEmiters.size());
        if (opEmiters.size() == 0) {
            return;
        }
        insert_inst();
        for(auto op : opEmiters)
        {
            op(encoderStream);
        }
        gasCounter = 0;
        opEmiters.clear();
    }


#define VISIT_OP(encoding, name, nameString, Imm, _4, _5, gas)  \
    I64 name(Imm imm) {                                         \
        gasCounter += gas;                                      \
        opEmiters.push_back(                                    \
                [imm](CodeStream *codeStream){                  \
                codeStream->name(imm); });                      \
        return 0;                                               \
    }
    ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VISIT_OP)
#undef VISIT_OP

    I64 unknown(Opcode) {
        return 0;
    }

    void insert_inst()
    {
        // BUG : type converting forcefully
        //* https://webassembly.github.io/spec/core/syntax/values.html#syntax-int
        //* https://github.com/emscripten-core/emscripten/issues/7637
        wavmAssert(gasCounter < UINT32_MAX);

        U32 gas_low = U32(gasCounter & UINT32_MAX);
        U32 gas_high = U32(gasCounter>>32);
        encoderStream->i32_const({I32(gas_low)});
        encoderStream->i32_const({I32(gas_high)});
        encoderStream->call({addGasFuncIndex});
    }

	I64 block(ControlStructureImm imm)
    {
        put_trap();
        encoderStream->block(imm);

	switch(Opcode::block)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};

        pushControlStack(ControlContext::Type::block, "");
        return 0;
    }

	I64 loop(ControlStructureImm imm)
    {
        put_trap();
        encoderStream->loop(imm);
        //gasCounter += gas;
	switch(Opcode::loop)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        pushControlStack(ControlContext::Type::loop, "");
        return 0;
    }
	I64 if_(ControlStructureImm imm)
    {
        put_trap();
        encoderStream->if_(imm);
        //gasCounter += gas;
	switch(Opcode::if_)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        pushControlStack(ControlContext::Type::ifThen, "");
        return 0;
    }

	// If an else or end opcode would signal an end to the unreachable code, then pass it through to
	// the IR emitter.
	I64 else_(NoImm imm)
	{
        put_trap();
        encoderStream->else_(imm);
        //gasCounter += gas;
	switch(Opcode::else_)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        controlStack.back().type = ControlContext::Type::ifElse;
        return 0;
	}

	I64 end(NoImm imm)
	{
        put_trap();
        encoderStream->end(imm);
        //gasCounter += gas;
	switch(Opcode::end)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        controlStack.pop_back();
        return 0;
	}

	I64 try_(ControlStructureImm imm)
    {
        put_trap();
        encoderStream->try_(imm);
        //gasCounter += gas;
	switch(Opcode::try_)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        pushControlStack(ControlContext::Type::try_, "");
        return 0;
    }
	I64 catch_(ExceptionTypeImm imm)
	{
        put_trap();
        encoderStream->catch_(imm);
        //gasCounter += gas;
	switch(Opcode::catch_)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        controlStack.back().type = ControlContext::Type::catch_;
        return 0;
	}
	I64 catch_all(NoImm imm)
	{
        put_trap();
        encoderStream->catch_all(imm);
        //gasCounter += gas;
	switch(Opcode::catch_all)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_CONTROL_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        controlStack.back().type = ControlContext::Type::catch_;
        return 0;
	}

    I64 unreachable(NoImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->unreachable(imm); });
        return 0;
    }

    I64 br(BranchImm imm)
    {
        put_trap();
        encoderStream->br(imm);
        //gasCounter += gas;
	switch(Opcode::br)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 br_if(BranchImm imm)
    {
        put_trap();
        encoderStream->br_if(imm);
        //gasCounter += gas;
	switch(Opcode::br_if)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 br_table(BranchTableImm imm)
    {
        put_trap();
        encoderStream->br_table(imm);
        //gasCounter += gas;
	switch(Opcode::br_table)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 return_(NoImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->return_(imm); });
	switch(Opcode::return_)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 call(FunctionImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->call(imm); });
	switch(Opcode::call)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 call_indirect(CallIndirectImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->call_indirect(imm); });
	switch(Opcode::call_indirect)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 drop(NoImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->drop(imm); });
	switch(Opcode::drop)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 select(NoImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->select(imm); });
	switch(Opcode::select)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 local_set(GetOrSetVariableImm<false> imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->local_set(imm); });
	switch(Opcode::local_set)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 local_get(GetOrSetVariableImm<false> imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->local_get(imm); });
	switch(Opcode::local_get)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 local_tee(GetOrSetVariableImm<false> imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->local_tee(imm); });
	switch(Opcode::local_tee)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 global_set(GetOrSetVariableImm<true> imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->global_set(imm); });
	switch(Opcode::global_set)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 global_get(GetOrSetVariableImm<true> imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->global_get(imm); });
	switch(Opcode::global_get)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 table_get(TableImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->table_get(imm); });
	switch(Opcode::table_get)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 table_set(TableImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->table_get(imm); });
	switch(Opcode::table_set)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 table_grow(TableImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->table_grow(imm); });
	switch(Opcode::table_grow)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }


    I64 table_fill(TableImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->table_fill(imm); });
	switch(Opcode::table_fill)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 throw_(ExceptionTypeImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->throw_(imm); });
	switch(Opcode::throw_)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    I64 rethrow(RethrowImm imm)
    {
        opEmiters.push_back(
                [imm](CodeStream *codeStream){
                codeStream->rethrow(imm); });
	switch(Opcode::rethrow)
	{
#define VISIT_OPCODE(encoding, name, nameString, Imm, _4,_5, gas)                                         \
	case Opcode::name: gasCounter += gas; break;
		ENUM_PARAMETRIC_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
	};
        return 0;
    }

    void AddGas();

private:
    struct ControlContext
    {
        enum class Type : U8
        {
            function,
            block,
            ifThen,
            ifElse,
            loop,
            try_,
            catch_,
        };
        Type type;
        std::string lableId;
    };
    std::vector<ControlContext> controlStack;
    void pushControlStack(ControlContext::Type type, std::string lableId)
    {
        controlStack.push_back({type, lableId});
    }
};

void GasVisitor::AddGas()
{
    Serialization::ArrayOutputStream functionCodes;
    OperatorEncoderStream  encoder(functionCodes);
    encoderStream = new CodeValidationProxyStream<OperatorEncoderStream>(
            module, functionDef, encoder);

	OperatorDecoderStream decoder(functionDef.code);
	pushControlStack(
		ControlContext::Type::function, "");
	while(decoder && controlStack.size()){ decoder.decodeOp(*this); }
    encoderStream->finishValidation();
    functionDef.code = functionCodes.getBytes();
}

