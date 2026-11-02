#include "Domain/RelationshipText.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
namespace {
std::string TrimRelationshipText(const std::string& text){const auto start=text.find_first_not_of(" \t\r\n");return start==std::string::npos?"":text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);}
}
int TestRelationshipText()
{
    using namespace GirlsSpire;int checks=0,failed=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){++failed;std::cerr<<"FAIL RelationshipText: "<<why<<'\n';}};
    const auto path=std::filesystem::path(u8"" __FILE__).parent_path().parent_path().parent_path()/"specs/001-girls-spire/social-ending-dialogue-proposal.md";
    std::ifstream input(path);check(input.is_open(),"承認済み本文正本を読める");
    std::string line;int section=0,normal=0,special=0,endings=0,ending=-1;std::string title,page;
    const auto verifyEnding=[&]{if(ending<0)return;++endings;check(ending==endings-1,"後日談IDを維持");const auto* text=FindEndingText(ending);check(text!=nullptr,"全9編がある");if(text){check(text->Title==title,"題名が正本と完全一致");check(text->Page==TrimRelationshipText(page),"本文が段落を含め正本と完全一致");}};
    while(std::getline(input,line)){
        if(line.starts_with("## 通常交流")){section=1;continue;}
        if(line.starts_with("## 専用交流")){section=2;continue;}
        if(line.starts_with("## 後日談")){section=3;continue;}
        if(section==3){
            if(line.starts_with("### ID ")){verifyEnding();ending=line[7]-'0';const auto colon=line.find("：");title=TrimRelationshipText(line.substr(colon+3));page.clear();}
            else if(ending>=0)page+=line+'\n';
            continue;
        }
        if(line.size()<3 || line[0]!='|' || line[2]<'1' || line[2]>'8')continue;
        std::vector<std::string> columns;std::istringstream stream(line);std::string field;while(std::getline(stream,field,'|'))columns.push_back(TrimRelationshipText(field));
        const int actor=line[2]-'0';const auto* text=FindSocialDialogue(actor);check(text!=nullptr,"全8人に交流本文がある");
        if(section==1){++normal;check(actor==normal && columns.size()==6,"通常交流IDと列数を維持");if(text && columns.size()==6){check(text->Opening==columns[2],"通常交流の台詞が正本と一致");for(int choice=0;choice<3;++choice)check(text->Replies[choice]==columns[choice+3],"返答の順と本文が正本と一致");}}
        if(section==2){++special;check(actor==special && columns.size()==3,"専用交流IDと列数を維持");if(text && columns.size()==3)check(text->Special==columns[2],"専用交流本文が正本と一致");}
    }
    verifyEnding();check(normal==8 && special==8 && endings==9,"全8人/9編を漏れなく照合");
    for(int id:{std::numeric_limits<int>::min(),-1,9,100,std::numeric_limits<int>::max()}){check(!FindSocialDialogue(id),"不正交流IDを拒否");check(!FindEndingText(id),"不正後日談IDを拒否");}
    check(!FindSocialDialogue(0),"主人公を交流相手にしない");
    std::cout<<"RelationshipText: "<<checks<<" checks, "<<failed<<" failed\n";return failed;
}
