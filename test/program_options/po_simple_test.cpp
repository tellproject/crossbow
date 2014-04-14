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
                             value<'s'>("string", str),
                             value<'f'>("foo", foo));
    print_help(std::cout, opts);
    auto idx = parse(opts, argc, argv);
    auto fooVal = opts.get<'f'>();
    all = opts.get<'a'>();
    b = opts.get<'b'>();
    bar = opts.get<-1>();
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

