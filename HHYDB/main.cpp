#include "importData.h"

int yyparse (void);

int main() {
    importTablesData('|');
    cout << "*** Tables imported ***" << endl; 
    yyparse();
    return 0;
}
