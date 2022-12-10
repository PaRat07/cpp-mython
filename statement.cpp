#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        closure[var_name_] = var_value_->Execute(closure, context);
        return closure[var_name_];
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
            : var_name_(std::move(var))
            , var_value_(std::move(rv))
    {
    }

    VariableValue::VariableValue(const std::string& var_name)
            : dotted_ids_({var_name})
    {
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
            : dotted_ids_(std::move(dotted_ids))
    {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
        if (!closure.count(dotted_ids_.front())) {
            throw runtime_error("this variable doesn't exist");
        }
        string x = dotted_ids_.front();
        runtime::ObjectHolder *ptr = &closure.at(dotted_ids_.front());
        for (auto it = dotted_ids_.begin() + 1; it != dotted_ids_.end(); ++it) {
            ptr = &(ptr->TryAs<runtime::ClassInstance>()->Fields().at(*it));
        }
        return *ptr;
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return make_unique<Print>(make_unique<VariableValue>(name));
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(std::vector<std::unique_ptr<Statement>> args)
            : args_(std::move(args)){
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        bool is_first = true;
        for (auto &i : args_) {
            if (is_first) {
                is_first = false;
            }
            else {
                context.GetOutputStream() << " ";
            }
            ObjectHolder object = i->Execute(closure, context);
            if (object) {
                object->Print(context.GetOutputStream(), context);
            }
            else {
                context.GetOutputStream() << "None";
            }
        }
        context.GetOutputStream() << "\n";
        return runtime::ObjectHolder::None();
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                           std::vector<std::unique_ptr<Statement>> args)
            : object_(std::move(object))
            , method_(std::move(method))
            , args_(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        std::vector<runtime::ObjectHolder> args;
        for (const auto &i : args_) {
            args.push_back(i->Execute(closure, context));
        }
        return object_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(method_, args, context);
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        ostringstream buf;
        ObjectHolder object = argument_->Execute(closure, context);
        if (object) {
            object->Print(buf, context);
        }
        else {
            buf << "None";
        }
        return ObjectHolder::Own(runtime::String{buf.str()});
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        ObjectHolder object1 = lhs_->Execute(closure, context);
        ObjectHolder object2 = rhs_->Execute(closure, context);
        if (object1.TryAs<runtime::Number>() && object2.TryAs<runtime::Number>()) {
            return ObjectHolder::Own(runtime::Number{object1.TryAs<runtime::Number>()->GetValue() + object2.TryAs<runtime::Number>()->GetValue()});
        }
        else if (object1.TryAs<runtime::String>() && object2.TryAs<runtime::String>()) {
            return ObjectHolder::Own(runtime::String{object1.TryAs<runtime::String>()->GetValue() + object2.TryAs<runtime::String>()->GetValue()});
        }
        else if (object1.TryAs<runtime::ClassInstance>()
                 && object1.TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1)) {
            return object1.TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, {object2}, context);
        }
        else {
            throw runtime_error("non-summable types");
        }
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        ObjectHolder object1 = lhs_->Execute(closure, context);
        ObjectHolder object2 = rhs_->Execute(closure, context);
        if (object1.TryAs<runtime::Number>() && object2.TryAs<runtime::Number>()) {
            return ObjectHolder::Own(runtime::Number{object1.TryAs<runtime::Number>()->GetValue() - object2.TryAs<runtime::Number>()->GetValue()});
        }
        else {
            throw runtime_error("incorrect types for subtraction");
        }
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        ObjectHolder object1 = lhs_->Execute(closure, context);
        ObjectHolder object2 = rhs_->Execute(closure, context);
        if (object1.TryAs<runtime::Number>() && object2.TryAs<runtime::Number>()) {
            return ObjectHolder::Own(runtime::Number{object1.TryAs<runtime::Number>()->GetValue() * object2.TryAs<runtime::Number>()->GetValue()});
        }
        else {
            throw runtime_error("incorrect types for multiplying");
        }
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        ObjectHolder object1 = lhs_->Execute(closure, context);
        ObjectHolder object2 = rhs_->Execute(closure, context);
        if (object1.TryAs<runtime::Number>() && object2.TryAs<runtime::Number>() && object2.TryAs<runtime::Number>()->GetValue() != 0) {
            return ObjectHolder::Own(runtime::Number{object1.TryAs<runtime::Number>()->GetValue() / object2.TryAs<runtime::Number>()->GetValue()});
        }
        else {
            throw runtime_error("incorrect types for subtraction");
        }
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        try {
            for (auto &i : instructions_) {
                i->Execute(closure, context);
            }
            return ObjectHolder::None();
        }
        catch (...) {
            throw;
        }
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw ReturnExeption{statement_->Execute(closure, context)};
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls)
            : cls_(std::move(cls)) {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
        string cls_name = cls_.TryAs<runtime::Class>()->GetName();
        closure[cls_name] = runtime::ObjectHolder::Share(*cls_.TryAs<runtime::Class>());
        return closure[cls_name];
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                     std::unique_ptr<Statement> rv)
            : object_(std::move(object))
            , field_name_(std::move(field_name))
            , rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        ObjectHolder object = object_.Execute(closure, context);
        object.TryAs<runtime::ClassInstance>()->Fields()[field_name_] = rv_->Execute(closure, context);
        return object.TryAs<runtime::ClassInstance>()->Fields()[field_name_];
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body)
            : condition_(std::move(condition))
            , if_body_(std::move(if_body))
            , else_body_(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        try {
            if (condition_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
                if_body_->Execute(closure, context);
            } else {
                if (else_body_.get() != nullptr) {
                    else_body_->Execute(closure, context);
                }
            }
            return ObjectHolder::None();
        }
        catch (...) {
            throw;
        }
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue() || rhs_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
            return ObjectHolder::Own(runtime::Bool{true});
        }
        else {
            return ObjectHolder::Own(runtime::Bool{false});
        }
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue() && rhs_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()) {
            return ObjectHolder::Own(runtime::Bool{true});
        }
        else {
            return ObjectHolder::Own(runtime::Bool{false});
        }
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool{!argument_->Execute(closure, context).TryAs<runtime::Bool>()->GetValue()});
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
            : BinaryOperation(std::move(lhs), std::move(rhs))
            , cmp_(std::move(cmp)) {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool{cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context)});
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
            : class_ptr_(&class_)
            , args_(std::move(args)) {
    }

    NewInstance::NewInstance(const runtime::Class& class_)
            : class_ptr_(&class_) {
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        runtime::ObjectHolder object = runtime::ObjectHolder::Own(runtime::ClassInstance{*class_ptr_});

        if (object.TryAs<runtime::ClassInstance>()->HasMethod(INIT_METHOD, args_.size())) {
            std::vector<ObjectHolder> executed_args;
            for (auto &i : args_) {
                executed_args.push_back(i->Execute(closure, context));
            }
            object.TryAs<runtime::ClassInstance>()->Call(INIT_METHOD, executed_args, context);
        }
        return object;
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
            : body_(std::move(body))
    {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
            return runtime::ObjectHolder::None();
        }
        catch (const ReturnExeption &except) {
            return except.returned_value;
        }
    }

}  // namespace ast