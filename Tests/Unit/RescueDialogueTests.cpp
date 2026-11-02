#include "Domain/RescueDialogue.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
int TestRescueDialogue()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RescueDialogue: "<<why<<'\n';}};
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path().parent_path()/"specs/001-girls-spire/rescue-dialogue-proposal.md";
    std::ifstream input(path);check(input.is_open(),"承認済み本文正本を読める");
    std::string line;int rows=0;
    while(std::getline(input,line)){
        if(line.size()<3 || line[0]!='|' || line[2]<'1' || line[2]>'8')continue;
        std::vector<std::string> columns;std::istringstream stream(line);std::string field;
        while(std::getline(stream,field,'|')){
            const auto start=field.find_first_not_of(" \t\r");
            columns.push_back(start==std::string::npos?"":field.substr(start,field.find_last_not_of(" \t\r")-start+1));
        }
        check(columns.size()==6,"承認表は人物・本文・3選択肢の列を持つ");if(columns.size()!=6)continue;
        const int actor=columns[1][0]-'0';++rows;check(actor==rows,"人物IDと選択順を維持");
        const auto* dialogue=FindRescueDialogue(actor);check(dialogue!=nullptr,"全8人に本文がある");if(!dialogue)continue;
        check(dialogue->Opening==columns[2],"本人の言葉が承認表と完全一致");
        for(int choice=0;choice<3;++choice)check(dialogue->Replies[choice]==columns[choice+3],"3選択肢が承認済みの順と本文に完全一致");
        check(FindRescueDialogue(actor)==dialogue,"検索は同じ不変カタログを返す");
    }
    check(rows==8,"承認済み8人を漏れなく照合");
    for(int id:{std::numeric_limits<int>::min(),-1,0,9,100,std::numeric_limits<int>::max()})check(!FindRescueDialogue(id),"主人公と不正IDを救出候補にしない");
    std::cout<<"RescueDialogue: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
