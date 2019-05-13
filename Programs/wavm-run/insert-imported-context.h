#pragma once

typedef CodeValidationProxyStream<OperatorEncoderStream> CodeStream;

struct ImportFunctionInsertVisitor
{
    typedef void Result;
    ImportFunctionInsertVisitor(IR::Module& irModule, std::string name) :
        module(irModule), exportName(name) {}

    ~ImportFunctionInsertVisitor() {}
    CodeStream* encoderStream;
    IR::Module& module;
    std::string exportName;
    Uptr insertedIndex;

#define VISIT_OP(encoding, name, nameString, Imm, _4, _5)       \
    Result name(Imm imm) {                                      \
         encoderStream->name(imm);                              \
    }
    ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VISIT_OP)
#undef VISIT_OP

    Result unknown(Opcode) {}
    Result block(ControlStructureImm imm)
    {
        encoderStream->block(imm);
        pushControlStack(ControlContext::Type::block, "");
    }

    Result loop(ControlStructureImm imm)
    {
        encoderStream->loop(imm);
        pushControlStack(ControlContext::Type::loop, "");
    }

    Result if_(ControlStructureImm imm)
    {
        encoderStream->if_(imm);
        pushControlStack(ControlContext::Type::ifThen, "");
    }

    Result else_(NoImm imm)
    {
        encoderStream->else_(imm);
        controlStack.back().type = ControlContext::Type::ifElse;
    }

    Result end(NoImm imm)
    {
        encoderStream->end(imm);
        controlStack.pop_back();
    }

    Result try_(ControlStructureImm imm)
    {
        encoderStream->try_(imm);
        pushControlStack(ControlContext::Type::try_, "");
    }

    Result catch_(ExceptionTypeImm imm)
    {
        encoderStream->catch_(imm);
        controlStack.back().type = ControlContext::Type::catch_;
    }

    Result catch_all(NoImm imm)
    {
        encoderStream->catch_all(imm);
        controlStack.back().type = ControlContext::Type::catch_;
    }

    Result unreachable(NoImm imm)
    {
        encoderStream->unreachable(imm);
    }

    Result br(BranchImm imm)
    {
        encoderStream->br(imm);
    }

    Result br_if(BranchImm imm)
    {
        encoderStream->br_if(imm);
    }

    Result br_table(BranchTableImm imm)
    {
        encoderStream->br_table(imm);
    }

    Result return_(NoImm imm)
    {
        encoderStream->return_(imm);
    }

    // the only place should be updated
    Result call(FunctionImm imm)
    {
        if(imm.functionIndex >= insertedIndex)
            imm.functionIndex += 1;
        encoderStream->call(imm);
    }

    Result call_indirect(CallIndirectImm imm)
    {
        encoderStream->call_indirect(imm);
    }

    Result drop(NoImm imm)
    {
        encoderStream->drop(imm);
    }

    Result select(NoImm imm)
    {
        encoderStream->select(imm);
    }

    Result local_set(GetOrSetVariableImm<false> imm)
    {
        encoderStream->local_set(imm);
    }

    Result local_get(GetOrSetVariableImm<false> imm)
    {
        encoderStream->local_get(imm);
    }

    Result local_tee(GetOrSetVariableImm<false> imm)
    {
        encoderStream->local_tee(imm);
    }

    Result global_set(GetOrSetVariableImm<true> imm)
    {
        encoderStream->global_set(imm);
    }

    Result global_get(GetOrSetVariableImm<true> imm)
    {
        encoderStream->global_get(imm);
    }

    Result table_get(TableImm imm)
    {
        encoderStream->table_get(imm);
    }

    Result table_set(TableImm imm)
    {
        encoderStream->table_set(imm);
    }

    Result table_grow(TableImm imm)
    {
        encoderStream->table_grow(imm);
    }

    Result table_fill(TableImm imm)
    {
        encoderStream->table_fill(imm);
    }

    Result throw_(ExceptionTypeImm imm)
    {
        encoderStream->throw_(imm);
    }

    Result rethrow(RethrowImm imm)
    {
        encoderStream->rethrow(imm);
    }

    void AddImportedFunc();

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

/*
 *  add imported function add the tail of the function.imports
    - IR::Module::exports
    - IR::Module::elemSegments
    - IR::Module::startFunctionIndex
    - IR::FunctionImm in the byte code for call instructions
*/
void ImportFunctionInsertVisitor::AddImportedFunc()
{
    insertedIndex = module.functions.imports.size();
    //insert types
    module.types.push_back(FunctionType{{}, ValueType::i64});

    //right shift all the index in elemSegments
    for(auto& seg : module.elemSegments)
    {
        for (auto& elem : seg.elems)
        {
            if (elem.index >= insertedIndex)
            {
                elem.index += 1;
            }
        }
    }

    //update start function
    if (module.startFunctionIndex != UINTPTR_MAX
            && module.startFunctionIndex >=  module.functions.imports.size()) {
        module.startFunctionIndex += 1;
    }

    //update exports
    for (auto& export_ : module.exports) {
        export_.index += 1;
    }

    // insert imported function
    module.functions.imports.push_back(
            {{module.types.size() - 1}, std::move("env"), std::move(exportName)});

    for (int i = 0; i < module.functions.defs.size(); ++ i) {
        //update FunctionImm
        FunctionDef& functionDef = module.functions.defs[i];
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
        delete encoderStream;
        encoderStream = nullptr;
    }
}
