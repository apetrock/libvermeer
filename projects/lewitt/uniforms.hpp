#pragma once
#include <stack>
#include <map>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  namespace uniforms
  {
    GLM_TYPEDEFS;

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
        __sizes = other.__sizes;
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
                                              __sizes(std::move(other.__sizes)),
                                              __size_map(std::move(other.__size_map)),
                                              __offsets(std::move(other.__offsets)) {}

      structish &operator=(const structish &other)
      {
        return *this = structish(other);
      }

      structish &operator=(structish &&other) noexcept
      {
        __data = std::exchange(other.__data, nullptr);
        __sizes = std::move(other.__sizes);
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
        std::vector<size_t> __sizes = {sizeof(Types)...};
        std::size_t offset = 0;
        auto name_iter = names.begin();
        for (const auto &size : __sizes)
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
      std::vector<std::size_t> __sizes;
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

#define UNIFORM_STRUCT struct __uniforms

#define UNIFORM_SIZE_FUNC                              \
  virtual size_t size() { return sizeof(__uniforms); } \
  static_assert(sizeof(__uniforms) % 16 == 0);

#define UNIFORM_DATA_FUNC \
  virtual char *data() { return reinterpret_cast<char *>(&__uniforms); }

#define UNIFORM_SET_DEF(NAME, BODY)       \
  struct NAME BODY;                       \
  using NAME##_set = uniform_set_t<NAME>; \
  using NAME##_set_ptr = std::shared_ptr<NAME##_set>;

#define declUniformType(UNIFORM) \
  std::remove_reference<decltype(*UNIFORM)>::type::TYPE

#define uniform_size(UNIFORM) \
  sizeof(declUniformType(UNIFORM))

#define set_uniform_member(UNIFORM, MEMBER, VALUE) \
  UNIFORM->get_struct().MEMBER = VALUE;            \
  UNIFORM->enque_update(sizeof(VALUE), offsetof(declUniformType(UNIFORM), MEMBER));

// this needs some work
#define set_uniform_struct(UNIFORM, ...) \
  auto u_ref = *UNIFORM;                 \
  UNIFORM->get_struct() =                \
      decltype(u_ref)::TYPE tmp(__VA_ARGS__);

    ///////////////////////////////////////

    class uniform_set
    {
      // uniform set will know how to deal with a variable set of uniforms
      // figure this out with gpt
    public:
      using ptr = std::shared_ptr<uniform_set>;
      uniform_set() {}
      ~uniform_set() {}
      // (Just aliases to make notations lighter)

      virtual size_t size()
      {
        return 0;
      }

      virtual char *data()
      {
        return nullptr;
      }

      virtual const std::type_info &type() const = 0;
    };

    template <typename T>
    class uniform_set_t : public uniform_set
    {
    public:
      using TYPE = T;

      DEFINE_CREATE_FUNC(uniform_set_t)
      uniform_set_t() {}
      // uniform_set_t(T uniforms) : __uniforms(uniforms) {}
      ~uniform_set_t()
      {
        if (__uniform_buffer != nullptr)
          __uniform_buffer.destroy();
      }
      virtual const std::type_info &type() const override { return typeid(T); };
      T &get_struct()
      {
        return __uniforms;
      }
      T &get_struct() const { return __uniforms; }
      void set_struct(const T &uniforms) { __uniforms = uniforms; }

      virtual size_t size() { return sizeof(T); }

      template <typename U>
      void set_member(T U::*member, const U &value)
      {
        __uniforms.*member = value;
      }

      void init_uniform_buffer(wgpu::Device &device)
      {
        // Create uniform buffer
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = sizeof(T);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        bufferDesc.mappedAtCreation = false;
        __uniform_buffer = device.createBuffer(bufferDesc);
      }

      void enque_update(size_t offset, size_t size)
      {
        __update_queue.push({offset, size});
      }

      void update(wgpu::Queue &queue)
      {
        while (!__update_queue.empty())
        {
          size_t size = __update_queue.top()[1];
          size_t offset = __update_queue.top()[0];
          queue.writeBuffer(__uniform_buffer, offset, &__uniforms + offset, size);
          __update_queue.pop();
        }
      }
      // virtual char *data() { return &__uniforms; }

      bool valid()
      {
        return __uniform_buffer != nullptr;
      }
      wgpu::Buffer buffer() { return __uniform_buffer; }
      UNIFORM_DATA_FUNC;
      // UNIFORM_SIZE_FUNC;

      wgpu::Buffer __uniform_buffer = nullptr;
      std::stack<std::array<size_t, 2>> __update_queue;
      T __uniforms;
    };

    UNIFORM_SET_DEF(my_uniforms, {
      mat4 projectionMatrix;
      mat4 viewMatrix;
      mat4 modelMatrix;
      vec4 color;
      vec3 cameraWorldPosition;
      float time;
    }) // scene_uniforms_group

    UNIFORM_SET_DEF(material_uniforms, {
      mat4 modelMatrix;
      vec4 color;
    }) // material_uniforms_group

    UNIFORM_SET_DEF(scene_uniforms, {
      // We add transform matrices
      mat4 projectionMatrix;
      mat4 viewMatrix;
      vec3 cameraWorldPosition;
      float time;
    }) // scene_uniforms_group

    // whelp to make this work we have to make a temp type
    using vec4x2 = std::array<vec4, 2>;
    UNIFORM_SET_DEF(lighting_uniforms, {
      vec4x2 directions;
      vec4x2 colors;

      // Material properties
      float hardness = 32.0f;
      float kd = 1.0f;
      float ks = 0.5f;
      float _pad[1];
    }) // lighting_uniforms_group

  }
}