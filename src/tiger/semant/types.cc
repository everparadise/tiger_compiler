#include "tiger/semant/types.h"

extern llvm::IRBuilder<> *ir_builder;
extern llvm::Module *ir_module;

namespace type {

NilTy NilTy::nilty_;
IntTy IntTy::intty_;
StringTy StringTy::stringty_;
VoidTy VoidTy::voidty_;
llvm::StructType *StringTy::string_llvm_type_ = nullptr;
llvm::PointerType *StringTy::string_ptr_llvm_type_ = nullptr;

Ty *Ty::ActualTy() { return this; }

Ty *NameTy::ActualTy() {
  assert(ty_ != this);
  return ty_->ActualTy();
}

bool Ty::IsSameType(Ty *expected) {
  Ty *a = ActualTy();
  Ty *b = expected->ActualTy();

  if ((typeid(*a) == typeid(NilTy) && typeid(*b) == typeid(RecordTy)) ||
      (typeid(*a) == typeid(RecordTy) && typeid(*b) == typeid(NilTy)))
    return true;
  return typeid(*a) == typeid(*b);
}

llvm::Type *Ty::GetLLVMType() {
  assert(0);
  return NULL;
}

llvm::Type *NilTy::GetLLVMType() { return ir_builder->getVoidTy(); }

llvm::Type *IntTy::GetLLVMType() { return ir_builder->getInt32Ty(); }

void StringTy::InitStringLLVMType() {
  llvm::Type *int32Type = llvm::Type::getInt32Ty(ir_builder->getContext());
  llvm::Type *UCharTy = llvm::Type::getInt8Ty(ir_builder->getContext());
  llvm::PointerType *CharsPtrType = llvm::PointerType::get(UCharTy, 0);

  std::vector<llvm::Type *> structElements = {int32Type, CharsPtrType};
  string_llvm_type_ = llvm::StructType::create(ir_builder->getContext(),
                                               structElements, "string");
  string_ptr_llvm_type_ = llvm::PointerType::get(string_llvm_type_, 0);
}

llvm::Type *StringTy::GetLLVMType() {
  if (string_llvm_type_ == nullptr) {
    InitStringLLVMType();
  }
  return string_ptr_llvm_type_;
}

llvm::Value *StringTy::CreateGlobalStringStructPtr(std::string str) {
  if (string_llvm_type_ == nullptr) {
    InitStringLLVMType();
  }
  llvm::Constant *intValue =
      llvm::ConstantInt::get(ir_builder->getInt32Ty(), str.length());
  llvm::Constant *chars = ir_builder->CreateGlobalStringPtr(str);

  std::vector<llvm::Constant *> initValues = {intValue, chars};
  llvm::Constant *structInit =
      llvm::ConstantStruct::get(string_llvm_type_, initValues);

  llvm::GlobalVariable *globalVar =
      new llvm::GlobalVariable(*ir_module,        // Module
                               string_llvm_type_, // Type
                               true,              // isConstant
                               llvm::GlobalVariable::ExternalLinkage, // Linkage
                               structInit, // Initializer
                               "str"       // Name
      );

  return globalVar;
}

llvm::Type *VoidTy::GetLLVMType() { return ir_builder->getVoidTy(); }

llvm::Type *RecordTy::GetLLVMType() {
  if (llvm_type_ == NULL) {
    llvm::StructType *structType =
        llvm::StructType::create(ir_builder->getContext(), "MyStruct");
    llvm_type_ = llvm::PointerType::get(structType, 0);

    std::vector<llvm::Type *> llvm_fields;
    for (auto field : fields_->GetList()) {
      llvm_fields.push_back(field->ty_->GetLLVMType());
    }

    structType->setBody(llvm_fields);
    llvm_type_ = llvm::PointerType::get(structType, 0);
  }
  return llvm_type_;
}

llvm::Type *ArrayTy::GetLLVMType() {
  // assert(0);
  return llvm::PointerType::get(ty_->GetLLVMType(), 0);
}

llvm::Type *ArrayTy::GetLLVMTypeWithLen(int len) {
  return llvm::ArrayType::get(ty_->GetLLVMType(), len);
}

llvm::Type *NameTy::GetLLVMType() { return ActualTy()->GetLLVMType(); }
} // namespace type
