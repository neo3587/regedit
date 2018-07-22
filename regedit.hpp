
#pragma once

#ifndef __NEO_REGEDIT_HPP__
#define __NEO_REGEDIT_HPP__


/*
	Header name: regedit.hpp
	Author: neo3587

	Notes:
		- Requires C++11 or higher
		- Some keys are redirections to another keys due to registry virtualization : https://docs.microsoft.com/es-es/windows/desktop/SysInfo/registry-virtualization
		- Some keys cannot be opened with write permissions
		- resource_list, full_resource_descriptor and resource_requirements_list types are part of the WDK : https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk,
			all these types requires a cast to the expected structure type defined on wdm.h :
				+ resource_list				 : https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/wdm/ns-wdm-_cm_resource_list
				+ full_resource_descriptor	 : https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/wdm/ns-wdm-_cm_full_resource_descriptor
				+ resoruce_requeriments_list : https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/wdm/ns-wdm-_io_resource_requirements_list
		- Be careful with what you're going to do, you may make a mess if you edit or delete some keys or values, always make a backup if you want to try 'weird things' : https://support.microsoft.com/en-us/help/322756/how-to-back-up-and-restore-the-registry-in-windows
*/



#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cctype>
#include <windows.h>




namespace neo {

	namespace __regedit_details {

		template<class ValType, class CmpIter, class IterChild, class GenFn>
		class iter {

			protected:

				HKEY _hkey = NULL;
				DWORD _pos = 0;

				iter(HKEY hkey, DWORD pos) : _hkey(hkey), _pos(pos) {}

			public:

				using iterator_category = std::bidirectional_iterator_tag; // random_access_iterator_tag
				using value_type		= std::pair<const std::string, ValType>;
				using difference_type	= ptrdiff_t;
				using pointer			= std::unique_ptr<value_type>;
				using reference			= value_type; // objects are created on the fly

				iter() {}
				iter(const iter&) = default;
				iter(iter&&) = default;

				IterChild& operator=(const iter& other) {
					_hkey = other._hkey;
					_pos = other._pos;
					return static_cast<IterChild&>(*this);
				}
				IterChild& operator=(iter&& other) {
					std::swap(_hkey, other._hkey);
					std::swap(_pos, other._pos);
					return static_cast<IterChild&>(*this);
				}

				bool operator==(const IterChild& other) const {
					return _pos == other._pos && _hkey == other._hkey;
				}
				bool operator!=(const IterChild& other) const {
					return !(*this == other);
				}
				bool operator==(const CmpIter& other) const {
					return _pos == other._pos && _hkey == other._hkey;
				}
				bool operator!=(const CmpIter& other) const {
					return !(*this == other);
				}

				IterChild& operator++() {
					++_pos;
					return static_cast<IterChild&>(*this);
				}
				IterChild operator++(int) {
					iter tmp(*this);
					++(*this);
					return static_cast<IterChild&>(tmp);
				}

				IterChild& operator--() {
					--_pos;
					return static_cast<IterChild&>(*this);
				}
				IterChild operator--(int) {
					iter tmp(*this);
					--(*this);
					return static_cast<IterChild&>(tmp);
				}

				reference operator*() {
					return GenFn()(_hkey, _pos);
				}
				const reference operator*() const {
					return const_cast<iter&>(*this).operator*();
				}

				pointer operator->() {
					return pointer(new reference(GenFn()(_hkey, _pos)));
				}
				const pointer operator->() const {
					return const_cast<iter&>(*this).operator->();
				}

		};

		enum class type : DWORD {
			none						= REG_NONE,
			sz							= REG_SZ,
			expand_sz					= REG_EXPAND_SZ,
			binary						= REG_BINARY,
			dword						= REG_DWORD,
			dword_little_endian			= REG_DWORD_LITTLE_ENDIAN,
			dword_big_endian			= REG_DWORD_BIG_ENDIAN,
			link						= REG_LINK,
			multi_sz					= REG_MULTI_SZ,
			resource_list				= REG_RESOURCE_LIST,
			full_resource_descriptor	= REG_FULL_RESOURCE_DESCRIPTOR,
			resource_requirements_list	= REG_RESOURCE_REQUIREMENTS_LIST,
			qword						= REG_QWORD,
			qword_little_endian			= REG_QWORD_LITTLE_ENDIAN
		};

		namespace read_overload { // GCC needs all this instead just a simple 'auto' return

			template<type T>
			using _return_t =
				typename std::conditional<T == type::none,							void*,
				typename std::conditional<T == type::sz,							std::string,
				typename std::conditional<T == type::expand_sz,						std::string,
				typename std::conditional<T == type::binary,						std::unique_ptr<BYTE>,
				typename std::conditional<T == type::dword,							DWORD,
				typename std::conditional<T == type::dword_big_endian,				DWORD,
				typename std::conditional<T == type::link,							std::wstring,
				typename std::conditional<T == type::multi_sz,						std::vector<std::string>,
				typename std::conditional<T == type::resource_list,					std::unique_ptr<BYTE>,
				typename std::conditional<T == type::full_resource_descriptor,		std::unique_ptr<BYTE>,
				typename std::conditional<T == type::resource_requirements_list,	std::unique_ptr<BYTE>,
				typename std::conditional<T == type::qword,							DWORD64,
				std::nullptr_t>::type>::type>::type>::type>::type>::type>::type>::type>::type>::type>::type>::type;

			std::unique_ptr<BYTE> read(HKEY hk, const std::string& name) {
				DWORD ty = 0, len = 0;
				RegQueryValueExA(hk, name.c_str(), NULL, NULL, NULL, &len);
				std::unique_ptr<BYTE> ptr(new BYTE[len]);
				RegQueryValueExA(hk, name.c_str(), NULL, &ty, ptr.get(), &len);
				return std::move(ptr);
			}

			template<type T> _return_t<T> read(HKEY hk, const std::string& name);
			template<> _return_t<type::none>						/* void*					*/ read<type::none>(HKEY hk, const std::string& name) {
				return static_cast<void*>(nullptr);
			}
			template<> _return_t<type::sz>							/* std::string				*/ read<type::sz>(HKEY hk, const std::string& name) {
				return std::string(reinterpret_cast<const char*>(read(hk, name).get()));
			}
			template<> _return_t<type::expand_sz>					/* std::string				*/ read<type::expand_sz>(HKEY hk, const std::string& name) {
				std::string str = read<type::sz>(hk, name);
				DWORD size = ExpandEnvironmentStringsA(str.c_str(), NULL, 0);
				std::unique_ptr<char> exp(new char[size]);
				ExpandEnvironmentStringsA(str.c_str(), exp.get(), size);
				return std::string(exp.get());
			}
			template<> _return_t<type::binary>						/* std::unique_ptr<BYTE>	*/ read<type::binary>(HKEY hk, const std::string& name) {
				return read(hk, name);
			}
			template<> _return_t<type::dword>						/* DWORD					*/ read<type::dword>(HKEY hk, const std::string& name) {
				return *reinterpret_cast<DWORD*>(read(hk, name).get());
			}
			template<> _return_t<type::dword_big_endian>			/* DWORD					*/ read<type::dword_big_endian>(HKEY hk, const std::string& name) {
				return read<type::dword>(hk, name);
				//std::unique_ptr<BYTE> pt = read<type::binary>();
				//return DWORD((pt.get()[0] << 24) | (pt.get()[1] << 16) | (pt.get()[2] << 16) | (pt.get()[3] << 0));
			}
			template<> _return_t<type::link>						/* std::wstring				*/ read<type::link>(HKEY hk, const std::string& name) {
				return std::wstring(reinterpret_cast<const wchar_t*>(read(hk, name).get()));
			}
			template<> _return_t<type::multi_sz>					/* std::vector<std::string>	*/ read<type::multi_sz>(HKEY hk, const std::string& name) {
				DWORD ty = 0, len = 0, off = 0;
				RegQueryValueExA(hk, name.c_str(), NULL, NULL, NULL, &len);
				std::unique_ptr<BYTE> ptr; (new BYTE[len]);
				RegQueryValueExA(hk, name.c_str(), NULL, &ty, ptr.get(), &len);

				std::vector<std::string> vec;
				while(off < len && *reinterpret_cast<const char*>(ptr.get() + off) != '\0') {
					vec.push_back(reinterpret_cast<const char*>(ptr.get() + off));
					off += vec.back().size() + 1;
				}
				return std::move(vec);
			}
			template<> _return_t<type::resource_list>				/* std::unique_ptr<BYTE>	*/ read<type::resource_list>(HKEY hk, const std::string& name) {
				return read(hk, name);
			}
			template<> _return_t<type::full_resource_descriptor>	/* std::unique_ptr<BYTE>	*/ read<type::full_resource_descriptor>(HKEY hk, const std::string& name) {
				return read(hk, name);
			}
			template<> _return_t<type::resource_requirements_list>	/* std::unique_ptr<BYTE>	*/ read<type::resource_requirements_list>(HKEY hk, const std::string& name) {
				return read(hk, name);
			}
			template<> _return_t<type::qword>						/* DWORD64					*/ read<type::qword>(HKEY hk, const std::string& name) {
				return *reinterpret_cast<DWORD64*>(read(hk, name).get());
			}
		}

		inline int _lcase_cmp(const char* s1, const char* s2) {
		    #ifdef _MSC_VER
			return _stricmp(s1, s2);
			#else // GCC having some linker problems even with strcasecmp (at least at my end)
			const char *p1 = s1, *p2 = s2;
            int result = 0;
            do {
                result = tolower(*p1) - tolower(*p2);
            } while(*p1++ != '\0' && *p2++ != '\0' && result == 0);
            return (std::min)(1, (std::max)(-1, result));
			#endif
		}

	}

	class regedit {

		private:

			HKEY _hkey = NULL;
			DWORD _mode = KEY_READ | KEY_WRITE;

			// O(log2 n) search, returns end position if fails
			DWORD _find_pos(const char* str) const {
				DWORD left = 0, right = 0, endp = 0;
				if(RegQueryInfoKeyA(_hkey, NULL, NULL, NULL, &endp, NULL, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
					return 0;

				right = endp;
				if(left != right) {
					char buff[255];
					while(left <= right) {
						DWORD blen = 255, pos = (left + right) >> 1;
						if(RegEnumKeyExA(_hkey, pos, buff, &blen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
							return endp;
						int cmp = __regedit_details::_lcase_cmp(str, buff);
						if(cmp > 0)
							left = pos + 1;
						else if(cmp < 0)
							right = pos - 1;
						else
							return pos;
					}
				}

				return endp;
			}
			
			struct _gen_fn {
				std::pair<std::string, regedit> operator()(HKEY hk, DWORD pos) const {
					char buff[255];
					DWORD blen = 255;
					RegEnumKeyExA(hk, pos, buff, &blen, NULL, NULL, NULL, NULL);
					return { buff, regedit(hk, buff) };
				}
			};

			#ifndef _MSC_VER
			static LONG RegDeleteTreeA(HKEY hk, LPCSTR pcstr) {
				typedef LONG(*_DLLRegDeleteTreeA)(HKEY, LPCSTR);
				static _DLLRegDeleteTreeA rgtafn = (_DLLRegDeleteTreeA)GetProcAddress(GetModuleHandleA("Advapi32.dll"), "RegDeleteTreeA");
				return rgtafn(hk, pcstr);
			}
			#endif

		public:

			// https://docs.microsoft.com/es-es/windows/desktop/SysInfo/registry-functions

			// Member Types:

			class iterator;
			class const_iterator;

			class iterator : public __regedit_details::iter<regedit, const_iterator, iterator, _gen_fn> {
				public:
					using __regedit_details::iter<regedit, const_iterator, iterator, _gen_fn>::iter;
					friend regedit;
					friend __regedit_details::iter<const regedit, iterator, const_iterator, _gen_fn>;
			};
			class const_iterator : public __regedit_details::iter<const regedit, iterator, const_iterator, _gen_fn> {
				public:
					using __regedit_details::iter<const regedit, iterator, const_iterator, _gen_fn>::iter;
					const_iterator(const iterator& other) : __regedit_details::iter<const regedit, iterator, const_iterator, _gen_fn>::iter(other._hkey, other._pos) {}
					const_iterator(iterator&& other) : __regedit_details::iter<const regedit, iterator, const_iterator, _gen_fn>::iter(other._hkey, other._pos) {}
					using __regedit_details::iter<const regedit, iterator, const_iterator, _gen_fn>::operator=;
					const_iterator& operator=(const iterator& other) {
						*this = static_cast<const const_iterator&>(other);
						return *this;
					}
					const_iterator& operator=(iterator&& other) {
						*this = static_cast<const_iterator&&>(std::forward<iterator>(other));
						return *this;
					}
					friend regedit;
					friend __regedit_details::iter<regedit, const_iterator, iterator, _gen_fn>;
			};
			using reverse_iterator			= std::reverse_iterator<iterator>;
			using const_reverse_iterator	= std::reverse_iterator<const_iterator>;

			struct hkey {
				static const HKEY classes_root;
				static const HKEY current_config;
				static const HKEY current_user;
				static const HKEY local_machine;
				static const HKEY users;
			};
			using type = __regedit_details::type;

			class value {

				public:

					HKEY _hkey = NULL;
					std::string _name;
					DWORD _mode = KEY_READ | KEY_WRITE;

				public:

					value() {}
					value(HKEY hk, const char* name = "", bool write_permision = true) : _name(name) {
						_mode = write_permision == true ? (KEY_READ | KEY_WRITE) : (KEY_READ);
						RegOpenKeyExA(hk, "", 0, _mode, &_hkey);
					}
					value(const value& other) {
						*this = other;
					}
					value(value&& other) {
						*this = std::forward<value>(other);
					}

					value& operator=(const value& other) {
						RegOpenKeyExA(other._hkey, "", 0, other._mode, &_hkey);
						_name = other._name;
						_mode = other._mode;
						return *this;
					}
					value& operator=(value&& other) {
						swap(other);
						return *this;
					}

					~value() {
						if(_hkey != NULL)
							RegCloseKey(_hkey);
					}

					std::unique_ptr<BYTE> read() const {
						return __regedit_details::read_overload::read(_hkey, _name);
					}
					template<type Ty>
					__regedit_details::read_overload::_return_t<Ty> read() const {
						return __regedit_details::read_overload::read<Ty>(_hkey, _name);
					}

					void write(const void* data, type ty, DWORD bytes) {
						RegSetValueExA(_hkey, _name.c_str(), 0, static_cast<DWORD>(ty), reinterpret_cast<const LPBYTE>(const_cast<void*>(data)), bytes);
					}

					template<type Ty, typename = typename std::enable_if<Ty == type::none>::type>
					void write() {
						write(nullptr, Ty, 0);
					}
					template<type Ty, typename = typename std::enable_if<Ty == type::sz || Ty == type::expand_sz>::type>
					void write(const std::string& val) {
						write(val.c_str(), Ty, val.size() + 1);
					}
					template<type Ty, typename = typename std::enable_if<Ty == type::binary>::type>
					void write(const uint8_t* val, size_t bytes) {
						write(val, Ty, static_cast<DWORD>(bytes));
					}
					template<type Ty, typename = typename std::enable_if<Ty == type::dword || Ty == type::dword_big_endian>::type>
					void write(DWORD val) {
						write(&val, Ty, sizeof(DWORD));
					}
					template<type Ty, typename = typename std::enable_if<Ty == type::link>::type>
					void write(const std::wstring& val) {
						write(val.c_str(), Ty, val.size() * 2 + 2);
					}
					template<type Ty, class InputIterator, typename = typename std::enable_if<Ty == type::multi_sz && std::is_convertible<decltype(*InputIterator()), std::string>::value>::type>
					void write(InputIterator left, InputIterator right) {
						std::vector<char> vec;
						for(InputIterator it = left; it != right; ++it) {
							for(size_t p = 0; (*it)[p] != '\0'; ++p)
								vec.push_back((*it)[p]);
							vec.push_back('\0');
						}
						vec.push_back('\0');
						write(vec.data(), Ty, vec.size());
					}
					template<type Ty, typename = typename std::enable_if<Ty == type::resource_list || Ty == type::full_resource_descriptor || Ty == type::resource_requirements_list>::type>
					void write(const void* val, size_t bytes) {
						write(val, Ty, static_cast<DWORD>(bytes));
					}
					template<type Ty, typename = typename std::enable_if<Ty == type::qword>::type>
					void write(DWORD64 val) {
						write(&val, Ty, sizeof(DWORD64));
					}

					regedit::type type() const {
						DWORD ty = 0;
						RegQueryValueExA(_hkey, _name.c_str(), NULL, &ty, NULL, NULL);
						return static_cast<regedit::type>(ty);
					}

					size_t size() const {
						DWORD len = 0;
						return static_cast<size_t>(RegQueryValueExA(_hkey, _name.c_str(), NULL, NULL, NULL, &len) == ERROR_SUCCESS ? len : 0);
					}

					void swap(value& other) {
						std::swap(_hkey, other._hkey);
						std::swap(_name, other._name);
						std::swap(_mode, other._mode);
					}

			};

			class values {

				private:

					HKEY& _hkey;

					// O(log2 n) search, returns end position if fails
					DWORD _find_pos(const char* str) const {
						DWORD left = 0, right = 0, endp = 0;
						if(RegQueryInfoKeyA(_hkey, NULL, NULL, NULL, NULL, NULL, NULL, &endp, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
							return 0;

						right = endp;
						if(left != right) {
							char buff[255];
							while(left <= right) {
								DWORD blen = 255, pos = (left + right) >> 1;
								if(RegEnumValueA(_hkey, pos, buff, &blen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
									return endp;
								int cmp = __regedit_details::_lcase_cmp(str, buff);
								if(cmp > 0)
									left = pos + 1;
								else if(cmp < 0)
									right = pos - 1;
								else
									return pos;
							}
						}

						return endp;
					}
					
					struct _gen_fn {
						std::pair<std::string, value> operator()(HKEY hk, DWORD pos) const {
							char buff[16383];
							DWORD blen = 16383;
							RegEnumValueA(hk, pos, buff, &blen, NULL, NULL, NULL, NULL);
							return {buff, value(hk, buff)};
						}
					};

					values(HKEY& hkey) : _hkey(hkey) {}

					friend regedit;

				public:

					// Constructors:

					values() = delete;
					values(const values&) = delete;
					values(values&&) = delete;

					// Member Types:

					class iterator;
					class const_iterator;

					class iterator : public __regedit_details::iter<value, const_iterator, iterator, _gen_fn> {
						public:
							using __regedit_details::iter<value, const_iterator, iterator, _gen_fn>::iter;
							friend values;
							friend __regedit_details::iter<const value, iterator, const_iterator, _gen_fn>;
					};
					class const_iterator : public __regedit_details::iter<const value, iterator, const_iterator, _gen_fn> {
						public:
							using __regedit_details::iter<const value, iterator, const_iterator, _gen_fn>::iter;
							const_iterator(const iterator& other) : __regedit_details::iter<const value, iterator, const_iterator, _gen_fn>::iter(other._hkey, other._pos) {}
							const_iterator(iterator&& other) : __regedit_details::iter<const value, iterator, const_iterator, _gen_fn>::iter(other._hkey, other._pos) {}
							using __regedit_details::iter<const value, iterator, const_iterator, _gen_fn>::operator=;
							const_iterator& operator=(const iterator& other) {
								*this = static_cast<const const_iterator&>(other);
								return *this;
							}
							const_iterator& operator=(iterator&& other) {
								*this = static_cast<const_iterator&&>(std::forward<iterator>(other));
								return *this;
							}
							friend values;
							friend __regedit_details::iter<value, const_iterator, iterator, _gen_fn>;
					};
					using reverse_iterator		 = std::reverse_iterator<iterator>;
					using const_reverse_iterator = std::reverse_iterator<const_iterator>;

					// Iterators:

					iterator begin() {
						return iterator(_hkey, 0);
					}
					const_iterator begin() const {
						return const_iterator(_hkey, 0);
					}
					const_iterator cbegin() const {
						return const_iterator(_hkey, 0);
					}

					iterator end() {
						return iterator(_hkey, static_cast<DWORD>(size()));
					}
					const_iterator end() const {
						return const_iterator(_hkey, static_cast<DWORD>(size()));
					}
					const_iterator cend() const {
						return const_iterator(_hkey, static_cast<DWORD>(size()));
					}

					reverse_iterator rbegin() {
						return reverse_iterator(end());
					}
					const_reverse_iterator rbegin() const {
						return const_reverse_iterator(end());
					}
					const_reverse_iterator crbegin() const {
						return const_reverse_iterator(cend());
					}

					reverse_iterator rend() {
						return reverse_iterator(begin());
					}
					const_reverse_iterator rend() const {
						return const_reverse_iterator(begin());
					}
					const_reverse_iterator crend() const {
						return const_reverse_iterator(cbegin());
					}

					// Element Access:

					value at(const std::string& val) {
						if(RegQueryValueExA(_hkey, val.c_str(), NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
							throw std::out_of_range("neo::regedit::values::at(): value doesn't exists");
						return value(_hkey, val.c_str());
					}
					const value at(const std::string& val) const {
						return const_cast<values&>(*this).at(val);
					}
					value operator[](const std::string& val) {
						try {
							return at(val);
						}
						catch(...) {
							RegSetValueExA(_hkey, val.c_str(), 0, REG_NONE, NULL, 0);
							return value(_hkey, val.c_str());
						}
					}
					const value operator[](const std::string& val) const {
						return const_cast<values&>(*this).operator[](val);
					}

					// Capacity:

					bool empty() const {
						return !size();
					}
					size_t size() const {
						DWORD size;
						return static_cast<size_t>(RegQueryInfoKeyA(_hkey, NULL, NULL, NULL, NULL, NULL, NULL, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS ? size : 0);
					}

					// Modifiers:

					std::pair<iterator, bool> insert(const std::string& val) {
						iterator it = find(val);
						if(it != end())
							return { it, false };
						RegSetValueExA(_hkey, val.c_str(), 0, REG_NONE, NULL, 0);
						return { find(val), true };
					}
					template<class InputIterator, typename = typename std::enable_if<std::is_convertible<decltype(*InputIterator()), std::string>::value>::type>
					iterator insert(InputIterator left, InputIterator right) {
						iterator it = end();
						while(left != right)
							it = insert(*left++).first; 
						return it;
					}
					void insert(std::initializer_list<std::string> il) {
						insert(il.begin(), il.end());
					}

					iterator erase(const_iterator pos) {
						if(RegDeleteValueA(_hkey, pos->first.c_str()) != ERROR_SUCCESS)
							throw std::logic_error("neo::regedit::values::erase(): trying to delete a value from an unvalid key");
						return iterator(_hkey, (std::min)(pos._pos, static_cast<DWORD>(size())));
					}
					size_t erase(const std::string& val) {
						const_iterator it = find(val);
						if(it == cend())
							return 0;
						erase(it);
						return 1;
					}
					iterator erase(const_iterator left, const_iterator right) {
						iterator it = end();
						while(left != right)
							it = erase(--right);
						return it;
					}

					void clear() {
						erase(begin(), end());
					}

					std::pair<iterator, bool> emplace(const std::string& val) {
						return insert(val);
					}

					// Operations:

					iterator find(const std::string& val) {
						return iterator(_hkey, _find_pos(val.c_str()));
					}
					const_iterator find(const std::string& val) const {
						return const_cast<values&>(*this).find(val);
					}

			} values;

			// Constructors:

			regedit() : values(_hkey) {}
			regedit(const regedit& other) : values(_hkey) {
				*this = other;
			}
			regedit(regedit&& other) : values(_hkey) {
				*this = std::forward<regedit>(other);
			}

			regedit(HKEY hkey, const std::string& key = "", bool write_permision = true) : values(_hkey) {
				open(hkey, key, write_permision);
			}

			regedit& operator=(const regedit& other) {
				RegOpenKeyExA(other._hkey, "", 0, other._mode, &_hkey); // generate a new handle to same key to avoid closing twice the same handle
				_mode = other._mode;
				return *this;
			};
			regedit& operator=(regedit&& other) {
				swap(other);
				return *this;
			};

			~regedit() {
				close();
			}

			// Iterators:

			iterator begin() {
				return iterator(_hkey, 0);
			}
			const_iterator begin() const {
				return const_iterator(_hkey, 0);
			}
			const_iterator cbegin() const {
				return const_iterator(_hkey, 0);
			}

			iterator end() {
				return iterator(_hkey, static_cast<DWORD>(size()));
			}
			const_iterator end() const {
				return const_iterator(_hkey, static_cast<DWORD>(size()));
			}
			const_iterator cend() const {
				return const_iterator(_hkey, static_cast<DWORD>(size()));
			}

			reverse_iterator rbegin() {
				return reverse_iterator(end());
			}
			const_reverse_iterator rbegin() const {
				return const_reverse_iterator(end());
			}
			const_reverse_iterator crbegin() const {
				return const_reverse_iterator(cend());
			}

			reverse_iterator rend() {
				return reverse_iterator(begin());
			}
			const_reverse_iterator rend() const {
				return const_reverse_iterator(begin());
			}
			const_reverse_iterator crend() const {
				return const_reverse_iterator(cbegin());
			}

			// Element Access:

			regedit at(const std::string& key) {
				regedit tmp(_hkey, key);
				if(!tmp.is_open())
					throw std::out_of_range("neo::regedit::at() key doesn't exists");
				return std::move(tmp);
			}
			const regedit at(const std::string& key) const {
				return const_cast<regedit&>(*this).at(key);
			}
			regedit operator[](const std::string& key) {
				HKEY hk;
				DWORD disp;
				if(RegCreateKeyExA(_hkey, key.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, _mode, NULL, &hk, &disp) != ERROR_SUCCESS)
					throw std::logic_error("neo::regedit::operator[](): trying to open or create a subkey to an unvalid key");
				return regedit(hk);
			}
			const regedit operator[](const std::string& key) const {
				return const_cast<regedit&>(*this).operator[](key);
			}

			// Capacity:

			bool empty() const {
				return !size();
			}
			size_t size() const {
				DWORD size;
				return static_cast<size_t>(RegQueryInfoKeyA(_hkey, NULL, NULL, NULL, &size, NULL, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS ? size : 0);
			}

			// Modifiers:

			bool open(HKEY hkey, const std::string& key = "", bool write_permision = true) {
				_mode = write_permision == true ? (KEY_READ | KEY_WRITE) : (KEY_READ);
				return RegOpenKeyExA(hkey, key.c_str(), 0, _mode, &_hkey) == ERROR_SUCCESS;
			}

			void close() {
				if(_hkey != NULL)
					RegCloseKey(_hkey);
				_hkey = NULL;
			}

			std::pair<iterator, bool> insert(const std::string& key) {
				HKEY hk;
				DWORD disp;
				if(RegCreateKeyExA(_hkey, key.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, _mode, NULL, &hk, &disp) != ERROR_SUCCESS)
					throw std::logic_error("neo::regedit::insert(): trying to insert a subkey to an unvalid key");
				return { find(key), (disp == REG_CREATED_NEW_KEY) };
			}
			template<class InputIterator, typename = typename std::enable_if<std::is_convertible<decltype(*InputIterator()), std::string>::value>::type>
			void insert(InputIterator left, InputIterator right) {
				while(left != right)
					insert(*left++);
			}
			void insert(std::initializer_list<std::string> il) {
				insert(il.begin(), il.end());
			}

			iterator erase(const_iterator pos) {
				if(RegDeleteTreeA(_hkey, pos->first.c_str()) != ERROR_SUCCESS)
					throw std::logic_error("neo::regedit::erase(): trying to delete a subkey from an unvalid key");
				return iterator(_hkey, pos._pos == 0 ? 0 : pos._pos - 1);
			}
			size_t erase(const std::string& key) {
				const_iterator it = find(key);
				if(it == cend())
					return 0;
				erase(it);
				return 1;
			}
			iterator erase(const_iterator left, const_iterator right) {
				iterator it = end();
				while(left != right)
					it = erase(--right);
				return it;
			}

			void swap(regedit& other) {
				std::swap(_hkey, other._hkey);
				std::swap(_mode, other._mode);
			}

			void clear() {
				erase(begin(), end());
			}

			std::pair<iterator, bool> emplace(const std::string& key) {
				return insert(key);
			}

			// Operations:

			iterator find(const std::string& key) {
				return iterator(_hkey, _find_pos(key.c_str()));
			}
			const_iterator find(const std::string& key) const {
				return const_cast<regedit&>(*this).find(key);
			}

			bool is_open() const {
				return _hkey != NULL;
			}

			static const char* type_to_string(type ty) {
				switch(ty) {
					case type::none:						return "none";
					case type::sz:							return "sz";
					case type::expand_sz:					return "expand_sz";
					case type::binary:						return "binary";
					case type::dword:						return "dword";
					case type::dword_big_endian:			return "dword_big_endian";
					case type::link:						return "link";
					case type::multi_sz:					return "multi_sz";
					case type::resource_list:				return "resource_list";
					case type::full_resource_descriptor:	return "full_resource_descriptor";
					case type::resource_requirements_list:	return "resource_requirements_list";
					case type::qword:						return "qword";
				}
				return "unknown";
			}

	};

	const HKEY regedit::hkey::classes_root		= HKEY_CLASSES_ROOT;
	const HKEY regedit::hkey::current_config	= HKEY_CURRENT_CONFIG;
	const HKEY regedit::hkey::current_user		= HKEY_CURRENT_USER;
	const HKEY regedit::hkey::local_machine		= HKEY_LOCAL_MACHINE;
	const HKEY regedit::hkey::users				= HKEY_USERS;


}



#endif

