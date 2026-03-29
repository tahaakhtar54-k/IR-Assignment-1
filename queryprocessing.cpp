#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cmath>
#include "include/json.hpp"

using namespace std;
using json = nlohmann::json;

//Porter Stemmer
static bool isVowel(const string& w, int i){
    char c=w[i];
    if(c=='a'||c=='e'||c=='i'||c=='o'||c=='u') return true;
    if(c=='y'&&i>0) return !isVowel(w,i-1);
    return false;
}
static int measure(const string& s){
    int m=0; bool inV=false;
    for(int i=0;i<(int)s.size();++i){
        if(isVowel(s,i)){inV=true;}
        else if(inV){++m;inV=false;}
    }
    return m;
}
static bool hasVowel(const string& s){
    for(int i=0;i<(int)s.size();++i) if(isVowel(s,i)) return true;
    return false;
}
static bool endsCVC(const string& w){
    int n=w.size();
    if(n<3) return false;
    char c3=w[n-1];
    return !isVowel(w,n-3)&&isVowel(w,n-2)&&!isVowel(w,n-1)
           &&c3!='w'&&c3!='x'&&c3!='y';
}
static bool hasSuffix(const string& w,const string& sfx,string& stem){
    if(w.size()<sfx.size()) return false;
    if(w.compare(w.size()-sfx.size(),sfx.size(),sfx)==0){
        stem=w.substr(0,w.size()-sfx.size()); return true;
    }
    return false;
}
string porterStem(const string& word){
    string w=word;
    if(w.size()<=2) return w;
    string stem;
    if(hasSuffix(w,"sses",stem)) w=stem+"ss";
    else if(hasSuffix(w,"ies",stem)) w=stem+"i";
    else if(!hasSuffix(w,"ss",stem)&&hasSuffix(w,"s",stem)) w=stem;
    bool extra=false;
    if(hasSuffix(w,"eed",stem)){if(measure(stem)>0) w=stem+"ee";}
    else if(hasSuffix(w,"ed",stem)){if(hasVowel(stem)){w=stem;extra=true;}}
    else if(hasSuffix(w,"ing",stem)){if(hasVowel(stem)){w=stem;extra=true;}}
    if(extra){
        string s2;
        if(hasSuffix(w,"at",s2)) w+="e";
        else if(hasSuffix(w,"bl",s2)) w+="e";
        else if(hasSuffix(w,"iz",s2)) w+="e";
        else{
            int n=w.size();
            if(n>1&&w[n-1]==w[n-2]&&!(w[n-1]=='l'||w[n-1]=='s'||w[n-1]=='z')&&!isVowel(w,n-1))
                w=w.substr(0,n-1);
            else if(measure(w)==1&&endsCVC(w)) w+="e";
        }
    }
    if(hasSuffix(w,"y",stem)&&hasVowel(stem)) w=stem+"i";
    struct{const char*s;const char*r;} s2[]={
        {"ational","ate"},{"tional","tion"},{"enci","ence"},{"anci","ance"},
        {"izer","ize"},{"abli","able"},{"alli","al"},{"entli","ent"},
        {"eli","e"},{"ousli","ous"},{"ization","ize"},{"ation","ate"},
        {"ator","ate"},{"alism","al"},{"iveness","ive"},{"fulness","ful"},
        {"ousness","ous"},{"aliti","al"},{"iviti","ive"},{"biliti","ble"},
    };
    for(int i=0;i<20;++i) if(hasSuffix(w,s2[i].s,stem)&&measure(stem)>0){w=stem+s2[i].r;break;}
    struct{const char*s;const char*r;} s3[]={
        {"icate","ic"},{"ative",""},{"alize","al"},{"iciti","ic"},
        {"ical","ic"},{"ful",""},{"ness",""},
    };
    for(int i=0;i<7;++i) if(hasSuffix(w,s3[i].s,stem)&&measure(stem)>0){w=stem+s3[i].r;break;}
    const char* s4[]={"al","ance","ence","er","ic","able","ible","ant",
                      "ement","ment","ent","ion","ou","ism","ate","iti",
                      "ous","ive","ize"};
    for(int i=0;i<19;++i)
        if(hasSuffix(w,s4[i],stem)){
            if(string(s4[i])=="ion"){
                if(!stem.empty()&&(stem[stem.size()-1]=='s'||stem[stem.size()-1]=='t')&&measure(stem)>1){w=stem;break;}
            } else if(measure(stem)>1){w=stem;break;}
        }
    if(hasSuffix(w,"e",stem)){
        int m=measure(stem);
        if(m>1) w=stem;
        else if(m==1&&!endsCVC(stem)) w=stem;
    }
    {int n=w.size();
     if(n>1&&w[n-1]=='l'&&w[n-2]=='l'&&measure(w)>1) w=w.substr(0,n-1);}
    return w;
}
//End Porter Stemmer

static string toLower(string s){
    transform(s.begin(),s.end(),s.begin(),::tolower);
    return s;
}
static string toUpper(string s){
    transform(s.begin(),s.end(),s.begin(),::toupper);
    return s;
}
static string stemWord(const string& w){ return porterStem(toLower(w)); }
static string trim(const string& s){
    int a=s.find_first_not_of(" \t\r\n");
    if(a==(int)string::npos) return "";
    int b=s.find_last_not_of(" \t\r\n");
    return s.substr(a,b-a+1);
}
static vector<string> split(const string& s){
    istringstream iss(s); vector<string> tok;
    string w; while(iss>>w) tok.push_back(w); return tok;
}

map<string,vector<int> >                          index;
map<string,map<string,vector<int> > >  positionalIndex;
map<string,string>                                docMap;

vector<int> intersect(const vector<int>& a, const vector<int>& b){
    vector<int> r; int i=0,j=0;
    while(i<(int)a.size()&&j<(int)b.size()){
        if(a[i]==b[j]){r.push_back(a[i]);++i;++j;}
        else if(a[i]<b[j])++i; else ++j;
    }
    return r;
}
vector<int> unionVec(const vector<int>& a, const vector<int>& b){
    vector<int> r; int i=0,j=0;
    while(i<(int)a.size()&&j<(int)b.size()){
        if(a[i]==b[j]){r.push_back(a[i]);++i;++j;}
        else if(a[i]<b[j]){r.push_back(a[i]);++i;}
        else{r.push_back(b[j]);++j;}
    }
    while(i<(int)a.size()) r.push_back(a[i++]);
    while(j<(int)b.size()) r.push_back(b[j++]);
    return r;
}
vector<int> getIndex(const string& w){
    map<string,vector<int> >::iterator it=index.find(w);
    return it!=index.end()?it->second:vector<int>();
}
vector<int> notQuery(const string& term){
    vector<int> notSet=getIndex(term);
    vector<int> result;
    for(map<string,string>::iterator it=docMap.begin();it!=docMap.end();++it){
        int id=stoi(it->first);
        if(!binary_search(notSet.begin(),notSet.end(),id))
            result.push_back(id);
    }
    sort(result.begin(),result.end());
    return result;
}
vector<int> proximityQuery(const string& t1,const string& t2,int k){
    vector<int> result;
    map<string,map<string,vector<int> > >::iterator it1=positionalIndex.find(t1);
    map<string,map<string,vector<int> > >::iterator it2=positionalIndex.find(t2);
    if(it1==positionalIndex.end()||it2==positionalIndex.end()) return result;

    map<string,vector<int> >& docs1=it1->second;
    map<string,vector<int> >& docs2=it2->second;

    for(map<string,vector<int> >::iterator dit=docs1.begin();dit!=docs1.end();++dit){
        const string& doc=dit->first;
        map<string,vector<int> >::iterator jt=docs2.find(doc);
        if(jt==docs2.end()) continue;
        const vector<int>& pos1=dit->second;
        const vector<int>& pos2=jt->second;
        bool matched=false;
        for(int pi=0;pi<(int)pos1.size()&&!matched;++pi){
            for(int pj=0;pj<(int)pos2.size()&&!matched;++pj){
                if(abs(pos1[pi]-pos2[pj])<=k+1){
                    result.push_back(stoi(doc));
                    matched=true;
                }
            }
        }
    }
    sort(result.begin(),result.end());
    return result;
}

vector<int> processQuery(const string& query, bool returnIds=false);

vector<int> processQuery(const string& rawQuery, bool returnIds){
    string query=trim(rawQuery);
    vector<string> tokens=split(query);
    vector<int> result;

    vector<int> allDocs;
    for(map<string,string>::iterator it=docMap.begin();it!=docMap.end();++it)
        allDocs.push_back(stoi(it->first));
    sort(allDocs.begin(),allDocs.end());

    //Parentheses handling
    size_t openPos=query.find('(');
    if(openPos!=string::npos){
        size_t closePos=query.find(')');
        string inner=trim(query.substr(openPos+1, closePos-openPos-1));
        vector<int> innerResult=processQuery(inner,true);

        string beforeParen=trim(query.substr(0,openPos));
        vector<string> beforeToks=split(beforeParen);

        if(beforeToks.size()==1 && toUpper(beforeToks[0])=="NOT"){
            for(int i=0;i<(int)allDocs.size();++i)
                if(!binary_search(innerResult.begin(),innerResult.end(),allDocs[i]))
                    result.push_back(allDocs[i]);
        }
        else if(beforeToks.size()==2){
            string t1=stemWord(beforeToks[0]);
            string op=toUpper(beforeToks[1]);
            if(op=="AND") result=intersect(getIndex(t1),innerResult);
            else          result=unionVec (getIndex(t1),innerResult);
        }
    }
    //Proximity: t1 t2 /k
    else if(tokens.size()==3 && tokens[2][0]=='/'){
        string t1=stemWord(tokens[0]), t2=stemWord(tokens[1]);
        int k=stoi(tokens[2].substr(1));
        result=proximityQuery(t1,t2,k);
    }
    //Single term
    else if(tokens.size()==1){
        result=getIndex(stemWord(tokens[0]));
    }
    //NOT t1
    else if(tokens.size()==2 && toUpper(tokens[0])=="NOT"){
        result=notQuery(stemWord(tokens[1]));
    }
    //t1 t2
    else if(tokens.size()==2 && toUpper(tokens[0])!="NOT"){
        result=unionVec(getIndex(stemWord(tokens[0])),getIndex(stemWord(tokens[1])));
    }
    //t1 AND/OR t2
    else if(tokens.size()==3){
        string t1=stemWord(tokens[0]), t2=stemWord(tokens[2]);
        if(toUpper(tokens[1])=="AND") result=intersect(getIndex(t1),getIndex(t2));
        else                          result=unionVec (getIndex(t1),getIndex(t2));
    }
    //t1 AND/OR NOT t2
    else if(tokens.size()==4 && toUpper(tokens[2])=="NOT"){
        string t1=stemWord(tokens[0]), t2=stemWord(tokens[3]);
        if(toUpper(tokens[1])=="AND") result=intersect(getIndex(t1),notQuery(t2));
        else                          result=unionVec (getIndex(t1),notQuery(t2));
    }
    //NOT t1 AND/OR t2
    else if(tokens.size()==4 && toUpper(tokens[0])=="NOT"){
        string t1=stemWord(tokens[1]), t2=stemWord(tokens[3]);
        if(toUpper(tokens[2])=="AND") result=intersect(notQuery(t1),getIndex(t2));
        else                          result=unionVec (notQuery(t1),getIndex(t2));
    }
    //t1 OP t2 OP t3
    else if(tokens.size()==5){
        string t1=stemWord(tokens[0]),t2=stemWord(tokens[2]),t3=stemWord(tokens[4]);
        string op1=toUpper(tokens[1]),op2=toUpper(tokens[3]);
        vector<int> temp;
        if(op1=="AND") temp=intersect(getIndex(t1),getIndex(t2));
        else           temp=unionVec (getIndex(t1),getIndex(t2));
        if(op2=="AND") result=intersect(temp,getIndex(t3));
        else           result=unionVec (temp,getIndex(t3));
    }

    if(returnIds) return result;
    return result;
}

void testQueries(){
    vector<string> queries;
    vector<set<string> > expected;

    auto addTest=[&](const string& q, const set<string>& e){
        queries.push_back(q); expected.push_back(e);
    };

    { set<string> e; e.insert("0");e.insert("1");e.insert("2");e.insert("3");e.insert("4");e.insert("5");e.insert("6");e.insert("8");e.insert("9");e.insert("10");e.insert("11");e.insert("12");e.insert("16");e.insert("17");e.insert("18");e.insert("19");e.insert("20");e.insert("21");e.insert("22");e.insert("24");e.insert("25");e.insert("26");e.insert("27");e.insert("28");e.insert("30");e.insert("32");e.insert("33");e.insert("34");e.insert("35");e.insert("36");e.insert("37");e.insert("39");e.insert("40");e.insert("41");e.insert("44");e.insert("45");e.insert("46");e.insert("47");e.insert("50");e.insert("51");e.insert("52");e.insert("53"); addTest("running",e); }
    { set<string> e; e.insert("31");e.insert("28");e.insert("37");e.insert("30");e.insert("7");e.insert("10");e.insert("14");e.insert("1");e.insert("6");e.insert("41");e.insert("15");e.insert("11");e.insert("29");e.insert("26");e.insert("52");e.insert("13");e.insert("32");e.insert("44");e.insert("4");e.insert("8");e.insert("22");e.insert("38");e.insert("48");e.insert("0");e.insert("47");e.insert("2");e.insert("23");e.insert("9");e.insert("3");e.insert("5");e.insert("12");e.insert("55"); addTest("not hammer",e); }
    { set<string> e; e.insert("37");e.insert("3");e.insert("19");e.insert("1");e.insert("9");e.insert("40");e.insert("51");e.insert("16");e.insert("15");e.insert("12");e.insert("31");e.insert("41");e.insert("39");e.insert("0");e.insert("53");e.insert("26");e.insert("29");e.insert("17");e.insert("24");e.insert("54");e.insert("7");e.insert("2");e.insert("5");e.insert("28");e.insert("42"); addTest("actions AND wanted",e); }
    { set<string> e; e.insert("31");e.insert("28");e.insert("50");e.insert("46");e.insert("37");e.insert("30");e.insert("54");e.insert("10");e.insert("18");e.insert("7");e.insert("1");e.insert("17");e.insert("41");e.insert("49");e.insert("6");e.insert("34");e.insert("36");e.insert("11");e.insert("45");e.insert("29");e.insert("26");e.insert("52");e.insert("13");e.insert("21");e.insert("24");e.insert("16");e.insert("25");e.insert("32");e.insert("33");e.insert("4");e.insert("44");e.insert("22");e.insert("8");e.insert("19");e.insert("40");e.insert("20");e.insert("38");e.insert("48");e.insert("0");e.insert("47");e.insert("27");e.insert("51");e.insert("43");e.insert("2");e.insert("35");e.insert("39");e.insert("9");e.insert("3");e.insert("5");e.insert("12");e.insert("55"); addTest("united OR plane",e); }
    { set<string> e; e.insert("29");e.insert("16");e.insert("4");e.insert("22");e.insert("37");e.insert("40");e.insert("42");e.insert("18");e.insert("1");e.insert("17");e.insert("41");e.insert("39");e.insert("9");e.insert("3"); addTest("pakistan OR afganistan OR aid",e); }
    { set<string> e; e.insert("18");e.insert("46");e.insert("54");e.insert("43");e.insert("4");e.insert("45");e.insert("50");e.insert("53");e.insert("47");e.insert("6");e.insert("51");e.insert("44"); addTest("biggest AND ( near OR box )",e); }
    { set<string> e; e.insert("18");e.insert("46");e.insert("4");e.insert("45");e.insert("50");e.insert("9");e.insert("47");e.insert("23");e.insert("54");e.insert("44");e.insert("25"); addTest("box AND ( united OR year )",e); }
    { set<string> e; e.insert("50");e.insert("53");e.insert("46");e.insert("37");e.insert("30");e.insert("54");e.insert("42");e.insert("18");e.insert("7");e.insert("1");e.insert("49");e.insert("41");e.insert("6");e.insert("36");e.insert("45");e.insert("26");e.insert("52");e.insert("44");e.insert("16");e.insert("4");e.insert("8");e.insert("19");e.insert("40");e.insert("48");e.insert("0");e.insert("47");e.insert("51");e.insert("43");e.insert("2");e.insert("35");e.insert("39"); addTest("biggest AND ( plane OR wanted OR hour)",e); }
    { set<string> e; e.insert("31");e.insert("28");e.insert("50");e.insert("53");e.insert("46");e.insert("37");e.insert("54");e.insert("42");e.insert("7");e.insert("10");e.insert("14");e.insert("18");e.insert("6");e.insert("49");e.insert("41");e.insert("15");e.insert("11");e.insert("45");e.insert("13");e.insert("21");e.insert("44");e.insert("16");e.insert("4");e.insert("8");e.insert("22");e.insert("40");e.insert("20");e.insert("38");e.insert("48");e.insert("47");e.insert("51");e.insert("43");e.insert("23");e.insert("39");e.insert("9");e.insert("3");e.insert("5");e.insert("12");e.insert("55"); addTest("NOT (united AND plane)",e); }
    { set<string> e; e.insert("1");e.insert("2");e.insert("3");e.insert("4");e.insert("5");e.insert("6");e.insert("7");e.insert("8");e.insert("9");e.insert("10");e.insert("11");e.insert("12");e.insert("14");e.insert("16");e.insert("17");e.insert("18");e.insert("19");e.insert("20");e.insert("21");e.insert("22");e.insert("24");e.insert("25");e.insert("26");e.insert("27");e.insert("28");e.insert("29");e.insert("30");e.insert("31");e.insert("32");e.insert("33");e.insert("34");e.insert("35");e.insert("36");e.insert("37");e.insert("39");e.insert("40");e.insert("41");e.insert("42");e.insert("43");e.insert("44");e.insert("45");e.insert("46");e.insert("47");e.insert("48");e.insert("49");e.insert("50");e.insert("51");e.insert("52");e.insert("53");e.insert("54"); addTest("Hillary Clinton",e); }
    { set<string> e; e.insert("6");e.insert("7");e.insert("44"); addTest("after years /1",e); }
    { set<string> e; e.insert("5");e.insert("32"); addTest("develop solutions /1",e); }
    { set<string> e; e.insert("20");e.insert("24");e.insert("39");e.insert("40");e.insert("51"); addTest("keep out /2",e); }

    int passed=0,total=(int)queries.size();
    for(int t=0;t<total;++t){
        vector<int> ids=processQuery(queries[t],true);
        set<string> got;
        for(int i=0;i<(int)ids.size();++i) got.insert(to_string(ids[i]));

        if(got==expected[t]){
            cout<<"\n PASSED: '"<<queries[t]<<"'\n";
            cout<<"  Found "<<ids.size()<<" document(s):\n";
            for(int i=0;i<(int)ids.size();++i){
                map<string,string>::iterator it=docMap.find(to_string(ids[i]));
                if(it!=docMap.end())
                    cout<<"    ["<<ids[i]<<"] "<<it->second<<"\n";
            }
            ++passed;
        } else {
            cout<<"\n FAILED: '"<<queries[t]<<"'\n";
            cout<<"  Got "<<ids.size()<<" document(s):\n";
            for(int i=0;i<(int)ids.size();++i){
                map<string,string>::iterator it=docMap.find(to_string(ids[i]));
                if(it!=docMap.end())
                    cout<<"    ["<<ids[i]<<"] "<<it->second<<"\n";
            }
            set<string> missing,extra;
            for(set<string>::iterator it=expected[t].begin();it!=expected[t].end();++it)
                if(!got.count(*it)) missing.insert(*it);
            for(set<string>::iterator it=got.begin();it!=got.end();++it)
                if(!expected[t].count(*it)) extra.insert(*it);
            if(!missing.empty()){
                cout<<"  Missing IDs: {";
                for(set<string>::iterator it=missing.begin();it!=missing.end();++it) cout<<*it<<" ";
                cout<<"}\n";
            }
            if(!extra.empty()){
                cout<<"  Extra IDs:   {";
                for(set<string>::iterator it=extra.begin();it!=extra.end();++it) cout<<*it<<" ";
                cout<<"}\n";
            }
        }
        cout<<"\n";
    }
    cout<<"==============================\n";
    cout<<passed<<"/"<<total<<" queries passed\n";
    cout<<"==============================\n";
}

//Interactive search console
void InteractiveSearch(){
    string query;
    while(true){
        cout<<"\nEnter query (or 'Quit' to quit): ";
        if(!getline(cin,query)) break;
        if(toLower(trim(query))=="Quit") break;
        vector<int> ids=processQuery(query,true);
        if(ids.empty()){cout<<"No documents found\n";continue;}
        cout<<"Found "<<ids.size()<<" documents:\n";
        for(int i=0;i<(int)ids.size();++i){
            map<string,string>::iterator it=docMap.find(to_string(ids[i]));
            if(it!=docMap.end()) cout<<"  - "<<it->second<<"\n";
        }
    }
}

int main(){
    // Load JSON files
    ifstream f1("inverted_index.json");
    if(!f1.is_open()){cerr<<"Cannot open inverted_index.json\n";return 1;}
    json jIndex; f1>>jIndex;

    ifstream f2("positional_index.json");
    if(!f2.is_open()){cerr<<"Cannot open positional_index.json\n";return 1;}
    json jPositional; f2>>jPositional;

    ifstream f3("doc_map.json");
    if(!f3.is_open()){cerr<<"Cannot open doc_map.json\n";return 1;}
    json jDocMap; f3>>jDocMap;

    // Populate inverted index
    for(json::iterator it=jIndex.begin();it!=jIndex.end();++it){
        string word=it.key();
        for(json::iterator jt=it.value().begin();jt!=it.value().end();++jt)
            index[word].push_back(jt->get<int>());
    }

    // Populate positional index
    for(json::iterator it=jPositional.begin();it!=jPositional.end();++it){
        string word=it.key();
        for(json::iterator jt=it.value().begin();jt!=it.value().end();++jt){
            string docId=jt.key();
            for(json::iterator kt=jt.value().begin();kt!=jt.value().end();++kt)
                positionalIndex[word][docId].push_back(kt->get<int>());
        }
    }

    // Populate doc map
    for(json::iterator it=jDocMap.begin();it!=jDocMap.end();++it)
        docMap[it.key()]=it.value().get<string>();

    // Run tests
    testQueries();

    // Uncomment if want interactive search:
    // InteractiveSearch();

    return 0;
}
