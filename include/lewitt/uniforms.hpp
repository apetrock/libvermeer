#pragma once
#include <stack>
#include <map>

// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  namespace uniforms
  {
    GLM_TYPEDEFS;
    // this unforturnately doesn't offer the interface that I want
    template <typename T, const char *NAME>
    struct member
    {
      using type = T;
      static constexpr char *name = NAME;
    };

    class structish
    {
    public:
      structish()
      {
      }

      // Destructor to free the allocated memory
      ~structish()
      {
        delete[] __data;
      }

      structish(const structish &other)
      {

        if (__data)
          delete[] __data;

        __size = other.__size;
        __size_map = other.__size_map;
        __offsets = other.__offsets;
        if (other.__data)
        {
          __data = new char[other.__size];
          std::copy(other.__data, other.__data + other.__size, __data);
        }
        else
          __data = nullptr;
      }

      structish(structish &&other) noexcept : __size(__size),
                                              __data(std::exchange(other.__data, nullptr)),
                                              __size_map(std::move(other.__size_map)),
                                              __offsets(std::move(other.__offsets)) {}

      structish &operator=(const structish &other)
      {
        return *this = structish(other);
      }

      structish &operator=(structish &&other) noexcept
      {
        __data = std::exchange(other.__data, nullptr);
        __offsets = std::move(__offsets);
        __size_map = std::move(__size_map);
        __size = other.__size;
        return *this;
      }

      template <typename... Types>
      void init(std::initializer_list<std::string> names)
      {
        if (names.size() != sizeof...(Types))
        {
          throw std::invalid_argument("Number of names must match the number of types");
        }
        // Initialize the offset map and allocate memory
        std::vector<size_t> sizes = {sizeof(Types)...};
        std::size_t offset = 0;
        auto name_iter = names.begin();
        for (const auto &size : sizes)
        {
          __size_map[*name_iter] = size;
          __offsets[*name_iter] = offset;
          offset += size;
          ++name_iter;
        }

        __size = offset;
        __data = new char[offset];
        std::memset(__data, 0, offset);
      }

      template <typename... NamedTypes>
      void init_types()
      {
        std::vector<size_t> sizes = {sizeof(NamedTypes::type)...};
        std::vector<std::string> names = {sizeof(NamedTypes::name)...};
        std::size_t offset = 0;
        int i = 0;
        for (const auto &size : sizes)
        {
          __size_map[names[i]] = size;
          __offsets[names[i]] = offset;
          offset += size;
          ++i;
        }

        __size = offset;
        __data = new char[offset];
        std::memset(__data, 0, offset);
      }

      // Set method to store values in the allocated memory
      template <typename T>
      void throw_if_invalid(const std::string &name) const
      {
        auto it = __offsets.find(name);
        if (it == __offsets.end())
        {
          throw std::invalid_argument("Invalid name: " + name);
        }

        auto it_s = __size_map.find(name);
        if (sizeof(T) != it_s->second)
        {
          throw std::invalid_argument("Type size mismatch for name: " + name);
        }
      }

      template <typename T>
      void set(const std::string &name, const T &val)
      {
        throw_if_invalid<T>(name);
        auto it = __offsets.find(name);
        std::memcpy(static_cast<char *>(__data) + it->second, &val, sizeof(T));
      }

      // Get method to retrieve values from the allocated memory
      template <typename T>
      T get(const std::string &name) const
      {

        throw_if_invalid<T>(name);
        auto it = __offsets.find(name);
        T val;
        std::memcpy(&val, static_cast<const char *>(__data) + it->second, sizeof(T));
        return val;
      }

      char *data() { return __data; }
      size_t size() const { return __size; }
      size_t offset(const std::string &mem) { return __offsets[mem]; }

    private:
      std::map<std::string, std::size_t> __size_map;
      std::map<std::string, std::size_t> __offsets;
      char *__data = nullptr;
      size_t __size = 0;
    };

    inline void test_structish()
    {
      // placeholder... should do some real tests.
      lewitt::uniforms::structish my_foo;
      my_foo.init<int, float, double>({"int_member", "float_member", "double_member"});
      my_foo.set("int_member", 42);
      my_foo.set("float_member", 3.14f);
      my_foo.set("double_member", 2.718);

      std::cout << "foo int_member: " << my_foo.get<int>("int_member") << std::endl;
      std::cout << "foo float_member: " << my_foo.get<float>("float_member") << std::endl;
      std::cout << "foo double_member: " << my_foo.get<double>("double_member") << std::endl;

      lewitt::uniforms::structish my_bar(my_foo);

      std::cout << "bar int_member: " << my_bar.get<int>("int_member") << std::endl;
      std::cout << "bar float_member: " << my_bar.get<float>("float_member") << std::endl;
      std::cout << "bar double_member: " << my_bar.get<double>("double_member") << std::endl;

      lewitt::uniforms::structish my_baz = my_bar;

      std::cout << "baz int_member: " << my_baz.get<int>("int_member") << std::endl;
      std::cout << "baz float_member: " << my_baz.get<float>("float_member") << std::endl;
      std::cout << "baz double_member: " << my_baz.get<double>("double_member") << std::endl;
    }
  }
}