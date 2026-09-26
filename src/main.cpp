#include<iostream>
#include<string>
using namespace std;

int main() {
    while(true) {
        cout<<"myshell> ";
        string s; 
        getline(cin, s);
        cout<<"You entered: "<<s<<endl;
    }
}