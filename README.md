# regedit
Windows register editor for C++

# Usage Example

```c++
#include "regedit.hpp"
#include <iostream>

using namespace std;


int main(int argc, char* argv[]) {

	// neo::regedit is very similar to a std::map

	neo::regedit reg(neo::regedit::hkey::current_user, "Software", true);

	reg = reg["_test_key"];

	for(int i = 0; i < 10; i++)
		reg.insert("key " + std::to_string(i));
	reg["key 0"].emplace("skey 0");
	reg["key 0"]["skey 1"];

	reg.values["sz_val"].write<neo::regedit::type::sz>("test_sz_string_value");
	reg.values.insert("dw_val").first->second.write<neo::regedit::type::dword>(12345);

	cout << "HKEY_CURRENT_USER\\Software\\_test_key:" << endl;
	cout << "  subkeys (" << reg.size() << ") :" << endl;
	for(neo::regedit::iterator it = reg.begin(); it != reg.end(); ++it) 
		cout << "    " << it->first << " (" << it->second.size() << ")" << endl;
	
	cout << "  values (" << reg.values.size() << ") :" << endl;
	for(neo::regedit::values::iterator it = reg.values.begin(); it != reg.values.end(); ++it)
		cout << "    " << it->first << " (" << neo::regedit::type_to_string(it->second.type()) << ")" << endl;

	return 0;
}
```
