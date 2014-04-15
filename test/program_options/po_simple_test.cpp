#include <crossbow/program_options.hpp>

using namespace crossbow::program_options;

int main(int argc, const char** argv)
{
    bool all = false;
    bool bar = false;
    bool b = false;
    char foo = '\0';
    crossbow::string str;
    auto opts
            = create_options(argv[0],
                             value<'a'>("all", all, tag::ignore_short<true>{}, tag::description{"All Test"}),
                             toggle<'b'>("all2", tag::ignore_long<true>{}),
                             toggle<-1>("bar", tag::ignore_short<true>{}),
                             value<'s'>("string", str, tag::callback([](crossbow::string& str){
                                std::cout << "Callback with " << str << " on -s" <<std::endl;})),
                             value<'f'>("foo", foo));
    std::cout << "Size of options: " << sizeof(opts) << std::endl;
    print_help(std::cout, opts);
    auto idx = parse(opts, argc, argv);
    auto fooVal = get<'f'>(opts);
    //all = opts.get<'a'>();
    all = get<'a'>(opts);
    //b = opts.get<'b'>();
    b = get<'b'>(opts);
    //bar = opts.get<-1>();
    bar = get<-1>(opts);
    static_assert(std::is_same<decltype(fooVal), char>::value, "fooVal should be char");
    std::cout << std::endl << "Options set:" << std::endl;
    std::cout << "\tall = " << all << std::endl
              << "\t-b = " << b << std::endl
              << "\tbar = " << bar << std::endl
              << "\t-s = " << str << std::endl
              << "\t-f = " << foo << std::endl;
    std::cout << std::endl << "Rest Arguments:" << std::endl;
    for (; idx < argc; ++idx) {
        std::cout << argv[idx] << std::endl;
    }
    return 0;
}

