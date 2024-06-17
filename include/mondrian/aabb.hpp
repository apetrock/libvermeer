
#ifndef __MONDIAN_AABB__
#define __MONDIAN_AABB__

#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <stack>
#include <memory>
#include <random>

#include "glm_typedefs.h"

using uint = unsigned int;
using uint2 = std::array<uint, 2>;
using real = float;

const uint UNULL = uint(-1);
namespace mondrian
{
  template <typename T>
  T sgn(T val) { return T(T(0) < val) - T(val < T(0)); }

  namespace ext
  {

    typedef std::array<vec3, 2> extents_t;
    const real inf_t = std::numeric_limits<real>::infinity();
    const vec3 inf_3(inf_t, inf_t, inf_t);

    extents_t init()
    {
      return {inf_3, -inf_3};
    }

    bool overlap(const extents_t &A, const extents_t &B)
    {
      if (greater_than(A[0], B[1]))
        return false;
      if (less_than(A[1], B[0]))
        return false;

      return true;
    }

    extents_t inflate(extents_t e, real eps)
    {
      vec3 deps(eps, eps, eps);
      e[0] -= deps;
      e[1] += deps;
      return e;
    }

    extents_t expand(const extents_t &e, const vec3 &x)
    {
      extents_t eout;
      eout[0] = min(e[0], x);
      eout[1] = max(e[1], x);
      return eout;
    }

    extents_t expand(const extents_t &eA, const extents_t &eB)
    {
      extents_t eout;
      eout = expand(eA, eB[0]);
      eout = expand(eout, eB[1]);
      return eout;
    }

    real distance(const extents_t &e, const vec3 &x)
    {
      extents_t eout;
      vec3 c = (e[0] + e[1]);
      c *= 0.5;
      return norm(x - c);
    }
  } // namespace ext

  // want to be able to use reals or doubles
  // float  = 2^10 1024.0
  // double = 2^20 1048576.0
  // #define SPREAD_BITS 1024.0

  // dump binary to a stream to use in a cout ie << dump_binary(i) << endl;
  std::string dump_binary(uint i)
  {
    std::string s;
    for (int j = 31; j >= 0; j--)
    {
      s += ((i & (1 << j)) ? '1' : '0');
    }
    return s;
  }
  // Expands a 10-bit integer into 30 bits
  // by inserting 2 zeros after each bit.
  inline uint expandBits(uint v)
  {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
  }

  inline uint scale(real x, real c)
  {
    return std::min(std::max(x * c, 0.0f), c - 1.0f);
  }

  // Calculates a 30-bit Morton code for the
  // given 3D point located within the unit cube [0,1].
  inline uint packVec3(real x, real y, real z)
  {
    // seems like we are losing two bits, could we
    // potentially use the 2/4 residual bits?
    x = scale(x, 1024.0f);
    y = scale(y, 1024.0f);
    z = scale(z, 1024.0f);
    // the residual might be
    //  (      x  +       4*y  +       2*z ) -
    //  (floor(x) + 4*floor(y) + 2*floor(z))
    uint xx = expandBits((uint)x);
    uint yy = expandBits((uint)y);
    uint zz = expandBits((uint)z);
    return xx * 4 + yy * 2 + zz;
  }

  inline int clz(uint i, uint j, const std::vector<int> &ids, const std::vector<int> &hash)
  {

    if (j < 0)
      return -1;
    if (j > ids.size() - 1)
      return -1;
    int code_i = hash[ids[i]];
    int code_j = hash[ids[j]];
    // std::cout << "     " << code_i << " " << dump_binary(code_i) << std::endl;
    // std::cout << "     " << code_j << " "<< dump_binary(code_j) << std::endl;
    // std::cout << "     " << __builtin_clz(code_i ^ code_j) << std::endl;
    return __builtin_clz(code_i ^ code_j);
  }

  uint find_split(int start, int end, const std::vector<int> &ids, const std::vector<int> &hash)
  {

    // uint first_code = hash[ids[start]];
    // uint last_code = hash[ids[end]];
    // if (first_code == last_code)
    //   return (first_code + last_code) >> 1;

    // uint common_prefix = std::countl_zero(first^last);
    int common_prefix_dist = clz(start, end, ids, hash);
    int last_prefix_dist = clz(end - 1, end, ids, hash);
    uint split = start;

    uint step = end - start;
    while (step > 1)
    {
      step = (step + 1) >> 1; // exponential decrease
      int new_split = split + step;
      if (new_split < end)
      {

        int new_prefix_dist = clz(start, new_split, ids, hash);

        if (new_prefix_dist > common_prefix_dist)
        {
          split = new_split;
          last_prefix_dist = new_prefix_dist;
        }
      }
    }

    return split;
  }

  uint2 find_range(const int &i, const std::vector<int> &ids, const std::vector<int> &hash)
  {
    uint N = ids.size();

    int dir = sgn(clz(i, i + 1, ids, hash) - clz(i, i - 1, ids, hash));
    int sig_min = clz(i, i - dir, ids, hash);

    int lmax = 2;
    while (clz(i, i + lmax * dir, ids, hash) > sig_min)
      lmax *= 2;

    uint l = 0;
    uint t = lmax;

    while (t >= 1)
    {
      t = t >> 1;
      if (clz(i, i + (l + t) * dir, ids, hash) > sig_min)
        l += t;
    }
    uint j = i + l * dir;

    return dir < 0 ? uint2{j, uint(i)} : uint2{uint(i), j};
  }

  struct radix_tree_node
  {
    uint start = UNULL;
    uint end = UNULL;
    uint split = UNULL; // split is the index of the last element in the left child
    uint parent = UNULL;
  };

  void dump_nodes(const std::vector<radix_tree_node> &nodes)
  {
    int i = 0;
    for (auto &n : nodes)
    {
      std::cout << "node[" << i++ << "]: " << n.start << " " << n.end << " " << n.split << " " << n.parent << std::endl;
    }
  }

  std::vector<radix_tree_node> build_tree(const std::vector<int> &ids, const std::vector<int> &hash)
  {
    std::vector<radix_tree_node> nodes(ids.size() + ids.size() - 1);
    uint leaf_start = ids.size() - 1;
    for (int i = 0; i < ids.size(); i++)
    {
      nodes[leaf_start + i].start = leaf_start + i;
      nodes[leaf_start + i].end = leaf_start + i + 1;
      nodes[leaf_start + i].split = UNULL;
      nodes[leaf_start + i].parent = UNULL;
    }

    for (int i = 0; i < ids.size() - 1; i++)
    {
      uint2 range = find_range(i, ids, hash);
      uint split = find_split(range[0], range[1], ids, hash);

      nodes[i].start = range[0];
      nodes[i].end = range[1];
      nodes[i].split = split;
      if (split == range[0])
        nodes[leaf_start + range[0]].parent = i;
      else
        nodes[split].parent = i;

      if (split + 1 == range[1])
        nodes[leaf_start + range[1]].parent = i;
      else
        nodes[split + 1].parent = i;
    }
#if 0 // dump nodes
  dump_nodes(nodes);
#endif
    return nodes;
  }

  void test_tree(const std::vector<radix_tree_node> &nodes, const std::vector<int> &ids, const std::vector<int> &hash)
  {
    std::vector<bool> visited(ids.size(), false);
    std::stack<uint> stack;
    stack.push(0);
    uint leaf_start = ids.size() - 1;
    bool no_asserts = true;
    while (stack.size() > 0)
    {
      uint i = stack.top();
      stack.pop();
      std::cout << "i: " << i << std::endl;
      // cout current node
      std::cout << "node["
                << i << "]: "
                << nodes[i].start << " "
                << nodes[i].end << " "
                << nodes[i].split << " "
                << nodes[i].parent << " " << std::endl;

      if (nodes[i].split + 0 == nodes[i].start ||
          nodes[i].split + 1 == nodes[i].end)
      {
        uint j0 = nodes[i].start;
        uint j1 = nodes[i].end;
        visited[leaf_start + j0] = true;
        visited[leaf_start + j1] = true;
        assert(nodes[leaf_start + j0].parent == i);
        assert(nodes[leaf_start + j1].parent == i);
        //
      }
      else
      {
        uint start_i = nodes[i].start;
        uint end_i = nodes[i].end;

        uint j0 = nodes[i].split;
        uint j1 = nodes[i].split + 1;
        // assert parents
        assert(nodes[j0].parent == i);
        assert(nodes[j1].parent == i);
        // assert rangings
        assert(nodes[j0].start >= nodes[i].start);
        assert(nodes[j0].end <= nodes[i].end);
        assert(nodes[j1].start >= nodes[i].start);
        assert(nodes[j1].end <= nodes[i].end);
        stack.push(j0);
        stack.push(j1);
      }
    }
    bool visited_all = false;
    for (int i = 0; i < visited.size(); i++)
    {
      if (!visited[i])
      {
        visited_all = false;
        break;
      }
    }
  }

  void unit_test_tree()
  {
    std::vector<int> hash = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    // std::vector<int> hash = {0, 1, 2, 3, 4, 5, 6, 7};

    std::vector<int> ids = hash;
    std::vector<radix_tree_node> nodes = build_tree(ids, hash);
    test_tree(nodes, ids, hash);
  }

  using extents_3 = std::array<vec3, 2>;

  template <int STRIDE>
  ext::extents_t calc_extents(int i, const std::vector<int> &indices,
                              const std::vector<vec3> &vertices)
  {
    ext::extents_t extents = ext::init();
    extents[0] = vertices[indices[STRIDE * i + 0]];
    extents[1] = extents[0];
    for (int k = 0; k < STRIDE; k++)
    {
      vec3 p = vertices[indices[STRIDE * i + k]];
      extents = ext::expand(extents, p);
    }

    return extents;
  }

  ext::extents_t pyramid(const ext::extents_t &a, const ext::extents_t &b)
  {
    vec3 mn = min(a[0], b[0]); // assume atomic min/max on gpu
    vec3 mx = max(a[1], b[1]);
    return {mn, mx};
  }

  // function object prototype that takes T A and B and returns T, to generically perform pyramid ops

  template <typename T>
  std::vector<T> build_pyramid(const std::vector<T> &data, int leaf_start, const std::vector<radix_tree_node> &nodes,
                               const std::function<T()> &init,
                               const std::function<T(const T &, const T &)> &op)
  {
    // THE GPU is probably going to need a max tree depth, but we can just do a max recursion rate of 32?
    const int max_depth = 32;
    std::vector<T> pyramid(nodes.size(), init());

    for (int i = 0; i < data.size(); i++)
    {
      // insert leaf then propagate up the pyramid
      int leaf_start = data.size() - 1;
      int ii = leaf_start + i;

      T data_i = data[i];

      pyramid[ii] = T(data_i);
      int parent = nodes[ii].parent;

      int j = 0;
      while (j < max_depth && parent != UNULL)
      {

        pyramid[parent] = op(pyramid[parent], data_i);
        data_i = T(pyramid[parent]);
        parent = nodes[parent].parent;

        j++;
      }
    }
    return pyramid;
  }

  void dump_cube_list()
  {
    for (int i = 0; i < 8; i++)
    {
      for (int j = 0; j < 3; j++)
      {
        int neighbor = i ^ (1 << j); // Flip j-th bit of i
        if (neighbor <= i)
          continue; // Avoid drawing the same line twice
        std::cout << ", " << i << ", " << neighbor;
      }
    }
    std::cout << std::endl;
  }

  const int cube_edges[24] = {0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3, 2, 6, 3, 7, 4, 5, 4, 6, 5, 7, 6, 7};
  const int cube_verts[8][3] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 1}, {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}};
  inline std::array<vec3, 8> create_cube_vertices(const ext::extents_t &extents)
  {
    vec3 xf0 = extents[0];
    vec3 xf1 = extents[1];
    return {vec3(xf0[0], xf0[1], xf0[2]),
            vec3(xf0[0], xf0[1], xf1[2]),
            vec3(xf0[0], xf1[1], xf0[2]),
            vec3(xf0[0], xf1[1], xf1[2]),
            vec3(xf1[0], xf0[1], xf0[2]),
            vec3(xf1[0], xf0[1], xf1[2]),
            vec3(xf1[0], xf1[1], xf0[2]),
            vec3(xf1[0], xf1[1], xf1[2])};
  }

  /*
  void log_extents(const ext::extents_t &extents, vec4 color = vec4(1.0, 0.0, 0.0, 1.0))
  {

    std::array<vec3, 8> vertices = create_cube_vertices(extents);
    for (int i = 0; i < 12; i++)
    {
      int v0 = cube_edges[2 * i + 0];
      int v1 = cube_edges[2 * i + 1];
      // logger::line(vertices[v0], vertices[v1], vec4(0.0, 0.0, 1.0, 1.0));
      //  alternatively using the cube_verts array
      auto [i0, j0, k0] = cube_verts[v0];
      auto [i1, j1, k1] = cube_verts[v1];
      logger::line(vec3(extents[i0][0], extents[j0][1], extents[k0][2]),
                   vec3(extents[i1][0], extents[j1][1], extents[k1][2]), color);
      // logger::line(vec3(cube_verts[v0][0], cube_verts[v0][1], cube_verts[v0][2]), vec3(cube_verts[v1][0], cube_verts[v1][1], cube_verts[v1][2]), vec4(0.0, 0.0, 1.0, 1.0));
    }
  }
  */
  /*
  void log_extents_to_parent(int id,
                             int leaf_start,
                             const std::vector<radix_tree_node> &nodes, const std::vector<ext::extents_t> &extents)
  {

    int parent = nodes[leaf_start + id].parent;
    int j = 0;
    const int max_depth = 32;
    log_extents(extents[leaf_start + id], vec4(1.0, 1.0, 0.0, 1.0));

    for (int i = nodes[parent].start; i < nodes[parent].end; i++)
    {
      std::cout << id << "  i: " << i << " p: " << parent << std::endl;
      log_extents(extents[leaf_start + i], vec4(0.0, 1.0, 0.0, 1.0));
    }

    while (j < max_depth && parent != UNULL)
    {
      log_extents(extents[parent]);
      parent = nodes[parent].parent;
      j++;
    }
  }
  */
  class test_case
  {
  public:
    typedef std::shared_ptr<test_case> ptr;
    static ptr create(int N, int stride) { return std::make_shared<test_case>(N, stride); }

    test_case(int N = 100000, int stride = 3)
    {
      _build(N, stride);
    };

    std::vector<vec3> get_random_points(const int &N)
    {
      std::uniform_real_distribution<real> dist(-1.0, 1.0);
      std::mt19937_64 re;
      auto scaled_rand_vec = [dist, re]() mutable
      {
        return vec3(dist(re), dist(re), dist(re));
      };
      std::vector<vec3> tpoints;
      std::generate_n(std::back_inserter(tpoints), N, scaled_rand_vec);
      return tpoints;
    }

    void _build(int N = 100000, int stride = 3)
    {
      //
      _stride = stride;
      int K = 0;
      _x = get_random_points(stride * N);
      _indices = std::vector<int>(stride * N, -1);
      for (int i = 0; i < stride * N; i++)
      {
        _indices[i] = i;
      }
    }

    int stride() { return _stride; }
    std::vector<vec3> &x() { return _x; }
    std::vector<int> &indices() { return _indices; }

    int stride() const { return _stride; }
    const std::vector<vec3> &x() const { return _x; }
    const std::vector<int> &indices() const { return _indices; }

    std::vector<vec3> _x;
    std::vector<int> _indices;
    int _stride = 3;
  };

  std::vector<vec3> get_cens(const test_case &test_case)
  {
    int stride = test_case.stride();
    const std::vector<vec3> x = test_case.x();
    const std::vector<int> indices = test_case.indices();
    int N = indices.size() / stride;
    std::vector<vec3> cens(N);
    for (int i = 0; i < N; i++)
    {
      vec3 cen(0.0, 0.0, 0.0);
      for (int j = 0; j < stride; j++)
      {
        cen += x[indices[stride * i + j]];
      }
      cen /= real(stride);
      cens[i] = cen;
    }
    return cens;
  }

  class aabb_build
  {
  public:
    typedef std::shared_ptr<aabb_build> ptr;

    static ptr create() { return std::make_shared<aabb_build>(); }

    aabb_build() { load_shell(); };

    void load_shell()
    {
      _test_case = test_case::create(100, 3);
      //__M = load_cube();
    }

    void step_dynamics(int frame)
    {
      unit_test_tree();
      test_case & M = *_test_case;

      std::vector<vec3> &x = M.x();
      std::vector<vec3> cens = get_cens(M);
      std::vector<int> m_indices = M.indices();

      // calc bounding box of cens
      vec3 mn = cens[0];
      vec3 mx = cens[0];
      for (int i = 0; i < cens.size(); i++)
      {
        mn = min(mn, cens[i]);
        mx = max(mx, cens[i]);
      }

      std::vector<int> hash(cens.size(), 0);
      std::vector<int> indices(cens.size(), 0);
      for (int i = 0; i < cens.size(); i++)
      {
        indices[i] = i;
        vec3 c = cens[i] - mn;
        vec3 n = mx - mn;
        c = div(c, n);
        hash[i] = packVec3(c[0], c[1], c[2]);
      }

      std::sort(indices.begin(), indices.end(), [&](int a, int b)
                { return hash[a] < hash[b]; });

      std::vector<radix_tree_node> nodes = build_tree(indices, hash);
      int r0 = nodes[nodes[nodes[0].split].split].start;
      int r1 = nodes[nodes[nodes[0].split].split].end;
      int leaf_start = indices.size() - 1;

      std::vector<extents_3> extents(m_indices.size() / 3);
      for (int i = 0; i < m_indices.size() / 3; i++)
      {
        extents[i] = calc_extents<3>(indices[i], m_indices, x);
        // log_extents(extents[i]);
      }

      std::vector<extents_3> ext = build_pyramid<extents_3>(
          extents, leaf_start, nodes,
          []()
          { return ext::init(); },
          [](const extents_3 &a, const extents_3 &b)
          { return pyramid(a, b); });

      int test_id = leaf_start + 1;
      // log_extents_to_parent(1000, leaf_start, nodes, ext);
      // log_extents_to_parent(1, leaf_start, nodes, ext);
      // log_extents_to_parent(2, leaf_start, nodes, ext);

      /*
      for (int i = 0; i < (indices.size() - 1); i++)
      {
        vec3 xf0 = cens[indices[i + 0]];
        vec3 xf1 = cens[indices[i + 1]];
        if (i > r0 && i < r1)
          logger::line(xf0, xf1, vec4(0.0, 1.0, 0.0, 1.0));
        else
          logger::line(xf0, xf1, vec4(1.0, 0.0, 0.0, 1.0));
      }
      */
    }

    void step(int frame)
    {
      step_dynamics(frame);
    }

    test_case::ptr _test_case;
  };

} // mondrian

#endif