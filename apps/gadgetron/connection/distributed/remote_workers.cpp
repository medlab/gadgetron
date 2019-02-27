#include "remote_workers.h"
#include "RemoteChannel.h"
#include <boost/asio/io_service.hpp>
#include <boost/process.hpp>
#include <boost/process/async.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/qi.hpp>

#include <string>

BOOST_FUSION_ADAPT_STRUCT(
    Gadgetron::Server::Distributed::Address,
    (std::string, ip)(std::string, port)
)

namespace {
    using namespace Gadgetron::Server;
    using namespace Gadgetron::Server::Distributed;

    std::vector<Worker> parse_remote_workers(std::string input)
    {
        namespace qi = boost::spirit::qi;
        namespace ascii = boost::spirit::ascii;
        using ascii::space;
        using qi::_1;
        using qi::char_;
        using qi::double_;
        using qi::lexeme;
        using qi::phrase_parse;

        auto first = input.begin(), last = input.end();

        qi::rule<decltype(first), std::string(), ascii::space_type> ipv6 = '[' >> lexeme[+(char_ - ']')] >> ']';
        qi::rule<decltype(first), Address(), ascii::space_type> address_rule =
                '"' >>
                (lexeme[+(char_ - (lexeme[':'] | lexeme[']'] | lexeme['[']))] | ipv6) >>
                ':' >>
                (lexeme[+(char_ - '"')]) >>
                '"';

        auto result = std::vector<Worker>{};
        bool r = phrase_parse(
                first,
                last,
                ( '[' >> address_rule % ',' >> ']' ),
                space,
                result
        );

        if (first != last || !r ) {
            GWARN_STREAM("Failed to parse worker list from discovery command: " << input);
            return std::vector<Worker>{};
        }

        return result;
    }
}

std::vector<Worker> Distributed::get_remote_workers()
{
    auto worker_discovery_command = std::getenv("GADGETRON_REMOTE_WORKER_COMMAND");
    if (!worker_discovery_command) return std::vector<Worker>{};

    std::future<std::string> output;
    boost::process::system(
            worker_discovery_command,
            boost::process::std_out > output,
            boost::process::std_err > boost::process::null,
            boost::asio::io_service{}
    );

    return parse_remote_workers(output.get());
}
