#include <tuple>
#include <numeric>
#include <optional>
#include <cxxabi.h> //gcc only ?
#include <fstream>
#include <map>
#include <vector>

namespace {
    static std::vector<std::string> splitString(const std::string& str, const std::string& delimiter)
    {
        std::vector<std::string> tokens;
        size_t pos, lastpos = 0;
        while ((pos = str.find(delimiter, lastpos)) != std::string::npos) {
            tokens.push_back(str.substr(lastpos, pos));
            lastpos =  pos + delimiter.length();
        }
        tokens.push_back(str.substr(lastpos, std::string::npos));
        return tokens;
    };

    //Checking if BetterArgs::Cmd items are derivered from BetterArgs::ArgumentDefinition.
    template < template <typename T> class base,typename derived>
    struct is_base_of_template_impl
    {
        template<typename T>
        static constexpr std::true_type  test(const base<T> *);
        static constexpr std::false_type test(...);
        using type = decltype(test(std::declval<derived*>()));
    };

    template < template <typename> class base,typename derived>
    using is_base_of_template = typename is_base_of_template_impl<base,derived>::type;
    
    
    template<class T>
    struct is_c_str
    : std::integral_constant<
        bool,
        std::is_same<char const *, typename std::decay<T>::type>::value ||
        std::is_same<char *, typename std::decay<T>::type>::value
    > {};

    #define DEF_STATICVAL(valname) \
        template <typename, typename = int> \
        struct hasStatic_##valname : std::false_type { };\
        \
        template <typename T>\
        struct hasStatic_##valname<T, decltype(T::valname, 0)>\
            : std::integral_constant<bool,\
                !std::is_member_pointer<decltype(&T::valname)>::value && is_c_str<decltype(T::valname)>::value>\
        { };


    DEF_STATICVAL(name);
    DEF_STATICVAL(description)
}

namespace BetterArgsImpl
{
    template<typename... T> class BetterArgsBase;
    template<typename... T> class Cmd;
    template<typename... T> class File;
};

namespace BetterArgs
{
    template<typename T> struct ArgumentDefinition {
        using type = T;
        T val;
        bool isPopulated;
    };
    struct Exception : public std::runtime_error { template<typename T> Exception(T arg) : std::runtime_error(arg) {} };
    
    template<typename... T> struct Types
    {
        using Base = BetterArgsImpl::BetterArgsBase<T...>;
        using Cmd = BetterArgsImpl::Cmd<T...>;
        using File = BetterArgsImpl::File<T...>;
    };
}

namespace BetterArgsImpl
{
    //Can this be solved any better?
    namespace TypeConvertion
    {
        template<typename T> T StringToT(std::string val)
        {
            return T();
        }
        template<> int StringToT(std::string val)
        {
            return std::stoi(val);
        }

        template<> double StringToT(std::string val)
        {
            return std::stod(val);
        }

        template<> std::string StringToT(std::string val)
        {
            return val;
        }

        //Complicated convertion for booleans, defaulting to true for flag usage.
        template<> bool StringToT(std::string val)
        {
            if(val == "true" || val == "") return true;
            else if(val == "false") return false;
            return StringToT<int>(val);
        }
    }
    
    template<typename... T> class BetterArgsBase : std::tuple<T...>
    {
        static_assert((is_base_of_template<BetterArgs::ArgumentDefinition, T>::value && ...), "Incorrect usage of BetterArgs::Cmd - use structures with BetterArgs::ArgumentDefinition as a base.");
        static_assert((hasStatic_name<T>::value && ...), "[!] One of the member classes has name implemented wrong - should be static constexpr char name[] = \"name\" ");
	    static_assert((hasStatic_description<T>::value && ...), "[!] One of the member classes has description implemented wrong - should be static constexpr char description[] = \"description\" ");
       public:
            template<typename Functor> void for_each(Functor f){
                ( f(std::get<T>(*this)), ...);
            }

            template<typename U> std::optional<typename U::type&> getOptionalValue(){
                return std::get<U>(*this).isPopulated ? std::optional<typename U::type>(std::get<U>(*this).val) : std::nullopt;
            }
            template<typename U> void set(U::type val)
            {
                auto& elem = std::get<U>(*this);
                elem.isPopulated = true;
                elem.val = val;
                return;
            }
            //returns std::vector<std::pair<std::string, std::string>> - with pair beeing name, description.
            static constexpr std::vector<std::pair<std::string, std::string> > getDescriptionVector()
            {
                return std::vector<std::pair<std::string, std::string> > {std::make_pair<std::string, std::string>(T::name, T::description)...};
            }
            template<typename... U> void checkMandatory()
            {
                static_assert((is_base_of_template<BetterArgs::ArgumentDefinition, U>::value && ...), "Incorrect usage of BetterArgs::BetterArgsBase::checkMandatory - use structures with BetterArgs::ArgumentDefinition as a base.");
                static_assert((hasStatic_name<U>::value && ...), "[!] One of the member classes has name implemented wrong - should be static constexpr char name[] = \"name\" ");
	            static_assert((hasStatic_description<U>::value && ...), "[!] One of the member classes has description implemented wrong - should be static constexpr char description[] = \"description\" ");
                auto throwCheckMandatoryElement = [&](auto t) -> void {
                    if(t.isMandatory && !t.isPopulated)
                    {
                        int unmangling_err;
                        auto unmangled = abi::__cxa_demangle(typeid(t.val).name(), 0, 0, &unmangling_err);
                        throw BetterArgs::Exception(std::string("BetterArgs : misssing mandatory argument \"") + t.name + "\"" + "("+std::string(unmangled)+")");
                    }
                };
                ( throwCheckMandatoryElement(std::get<T>(*this)), ...);
            }

            template<typename... U> void overrideWith(const BetterArgsBase<U...>& o)
            {
                static_assert((is_base_of_template<BetterArgs::ArgumentDefinition, U>::value && ...), "Incorrect usage of BetterArgs::BetterArgsBase::overrideWith - use structures with BetterArgs::ArgumentDefinition as a base.");
                static_assert((hasStatic_name<U>::value && ...), "[!] One of the member classes has name implemented wrong - should be static constexpr char name[] = \"name\" ");
	            static_assert((hasStatic_description<U>::value && ...), "[!] One of the member classes has description implemented wrong - should be static constexpr char description[] = \"description\" ");
                auto overrideArgImpl = [&](auto overArg)
                {
                    if(overArg.isPopulated && (std::is_same<decltype(overArg), T>::value || ...))
                    {
                        std::get<decltype(overArg)>(*this) = overArg;
                    }
                };
                (overrideArgImpl(std::get<U>(o)), ...);
            }

        protected:
            using std::tuple<T...>::operator=; //Not sure about it - could be a better way of passing ?
            
            template <typename U> U convertRawArg(std::map<std::string, std::string>& args)
            {
                U u;
                u.isPopulated = (args.find(U::name) != args.end());
                u.val = u.isPopulated ? TypeConvertion::StringToT<typename U::type>(args[U::name]) : decltype(u.val)();
                return u;
            }
    };
    template<typename... T> class Cmd : public BetterArgsBase<T...>
    {
        public:
            Cmd(int argc, char** argv)
            {
                std::map<std::string, std::string> rawArgs;
                for(int i = 1; i < argc ; i++)
                {
                    auto tokens = splitString(std::string(argv[i]), "=");
                    //Two or less tokens
                    if(!tokens.empty() && tokens.size() <= 2)
                        //No "=" means its a flag, so just add empty string.
                        rawArgs.emplace(tokens[0], tokens.size() == 2?tokens[1]:"");
                    else
                        //Add pair : tokens[0], "=".join(tokens[1:]);
                        rawArgs.emplace(tokens[0], std::accumulate(tokens.begin()+2, tokens.end(), tokens[1], [](const auto& sum, const auto& elem){return sum + "=" + elem;}));
                }
                BetterArgsBase<T...>::operator=(std::make_tuple<T...>(convertRawArg<T>(rawArgs)...));
            }
    };

    template<typename... T> class File : public BetterArgsBase<T...>
    {
        public:
            File(const std::string& fname)
            {
                std::ifstream f(fname);
                if(!f)
                    throw(BetterArgs::Exception("No such a file: " + fname));
                std::map<std::string, std::string> rawArgs;

                std::string line; 
                while (std::getline(f, line))
                {
                    auto tokens = splitString(line, "=");
                    //Two or less tokens
                    if(!tokens.empty() && tokens.size() <= 2)
                        //No "=" means its a flag, so just add empty string (?)
                        rawArgs.emplace(tokens[0], tokens.size() == 2?tokens[1]:"");
                    else
                        //Add pair : tokens[0], "=".join(tokens[1:]);
                        rawArgs.emplace(tokens[0], std::accumulate(tokens.begin()+2, tokens.end(), tokens[1], [](const auto& sum, const auto& elem){return sum + "=" + elem;}));
                }
                BetterArgsBase<T...>::operator=(std::make_tuple<T...>(convertRawArg<T>(rawArgs)...));
            }
    };
};