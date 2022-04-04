#include <iostream>
#include "BetterArgs.h"

namespace CmdArg
{
	//If these are not there - just use descriptions!
	struct asdf : BetterArgs::ArgumentDefinition<std::string> 
	{
		static constexpr char name[] = "asdf";
		static constexpr char description[] = "asdf-description";
	};

	struct asdf2 : BetterArgs::ArgumentDefinition<std::string> 
	{
		static constexpr char name[] = "asdf2";
		static constexpr char description[] = "asdf2-description";
	};

	struct Mandatory : BetterArgs::ArgumentDefinition<bool>
	{
		static constexpr char name[] = "mandatory";
		static constexpr char description[] = "To be checked for mandatory usage...";
	};
};

int main(int argc, char** argv)
{
	using baTypes = BetterArgs::Types<CmdArg::asdf, CmdArg::asdf2, CmdArg::Mandatory>;
	baTypes::Base ba;
	ba.set<CmdArg::asdf2>("123");
	ba.overrideWith(baTypes::File("./settings.txt"));
	ba.overrideWith(baTypes::Env());
	ba.overrideWith(baTypes::Cmd(argc, argv));
	ba.checkMandatory<CmdArg::Mandatory>();
	ba.for_each([&](auto elem){
			std::cout << "\t" << elem.name << " val:";
			if(elem.isPopulated) std::cout << elem.val; else  std::cout << "None";
			std::cout<< std::endl;
		});
	return 0;
}