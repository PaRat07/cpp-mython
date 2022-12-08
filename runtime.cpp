#include "runtime.h"

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
            : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (object.TryAs<Bool>()) {
            return object.TryAs<Bool>()->GetValue();
        }
        else if (object.TryAs<Number>()) {
            return object.TryAs<Number>()->GetValue();
        }
        else if (object.TryAs<String>()) {
            return !object.TryAs<String>()->GetValue().empty();
        }
        else {
            return false;
        }
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (HasMethod("__str__"s, 0)) {
            cls_ptr_->GetMethod("__str__")->body->Execute(closure_, context).Get()->Print(os, context);
        }
        else {
            os << cls_ptr_;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        const auto *ptr = cls_ptr_->GetMethod(method);
        if (ptr) {
            if (cls_ptr_->GetMethod(method)->formal_params.size() == argument_count) {
                return true;
            }
        }
        return false;
    }

    Closure& ClassInstance::Fields() {
        return closure_;
    }

    const Closure& ClassInstance::Fields() const {
        return closure_;
    }

    ClassInstance::ClassInstance(const Class& cls)
        : cls_ptr_(&cls)
    {
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
                                     const std::vector<ObjectHolder>& actual_args,
                                     Context& context) {
        const auto *method_body = cls_ptr_->GetMethod(method);
        if (!method_body || method_body->formal_params.size() != actual_args.size()) {
            throw runtime_error("hasn't got this method");
        }
        Closure method_closure;
        method_closure["self"] = ObjectHolder::Share(*this);
        auto it1 = method_body->formal_params.begin();
        auto it2 = actual_args.begin();
        for (;it1 != method_body->formal_params.end() && it2 != actual_args.end(); ++it1, ++it2) {
            method_closure[*it1] = *it2;
        }
        return method_body->body->Execute(method_closure, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : name_(name)
        , parent_(parent)
    {
        for (Method &method : methods) {
            methods_[method.name][method.formal_params.size()] = std::move(method);
        }
    }

    const Method* Class::GetMethod(const std::string& name) const {
        if (methods_.count(name)) {
            return &methods_.at(name).begin()->second;
        }
        else {
            if (parent_ == nullptr) {
                return nullptr;
            }
            else {
                return parent_->GetMethod(name);
            }
        }
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, Context& context) {
        os << "Class " << name_;
        (void)context;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs && !rhs) {
            return true;
        }
        else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
            return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
        }
        else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
            return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
        }
        else if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
            return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
        }
        else if (lhs.TryAs<ClassInstance>()) {
            return lhs.TryAs<ClassInstance>()->Call("__eq__"s, {rhs}, context).TryAs<Bool>()->GetValue();
        }
        else {
            throw runtime_error("incorrect comparing types");
        }
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs || !rhs) {
            throw runtime_error("incomparable types");
        }
        else if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
            return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
        }
        else if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
            return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
        }
        else if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
            return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
        }
        else if (lhs.TryAs<ClassInstance>()) {
            return lhs.TryAs<ClassInstance>()->Call("__lt__"s, {rhs}, context).TryAs<Bool>()->GetValue();
        }
        else {
            throw runtime_error("incorrect comparing types");
        }
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        try {
            return !Equal(lhs, rhs, context);
        }
        catch(...) {
            throw;
        }
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        try {
            return !Less(lhs, rhs, context) && NotEqual(lhs, rhs, context);
        }
        catch(...) {
            throw;
        }
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        try {
            return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
        }
        catch(...) {
            throw;
        }
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        try {
            return !Less(lhs, rhs, context);
        }
        catch(...) {
            throw;
        }
    }

}  // namespace runtime