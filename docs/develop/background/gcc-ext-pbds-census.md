# GCC `<ext/>` 扩展数据结构普查

> 基于 GCC 14 的 `/usr/include/c++/14/ext/` 目录，2026-06-10 实地扫描。

---

## 一、PBDS（Policy-Based Data Structures）— 核心宝藏

PBDS 是一个完整的、工业级的策略化数据结构框架，约 200 个头文件。
它通过 **Tag 分发** + **策略参数** 实现编译时多态，和 chaistl 的
`balance_policy` concept 是同一设计哲学的不同表达。

### 1.1 关联容器（`assoc_container.hpp`）

#### 1.1.1 树容器 `tree<K, V, Cmp, Tag, Node_Update, Alloc>`

| 模板参数 | 默认值 | 说明 |
|----------|--------|------|
| `Key` | — | 键类型 |
| `Mapped` | — | 映射类型；`null_type` = set |
| `Cmp_Fn` | `std::less<Key>` | 比较器 |
| `Tag` | `rb_tree_tag` | **树家族选择** |
| `Node_Update` | `null_node_update` | **节点更新策略** |
| `_Alloc` | `std::allocator<char>` | 分配器 |

**Tag 选择（树家族）**：

| Tag | 底层结构 | 特点 | 迭代器保证 |
|-----|----------|------|------------|
| `rb_tree_tag` | 红黑树 | 默认，最均衡 | `range_invalidation_guarantee` |
| `splay_tree_tag` | Splay 树 | 摊还 O(log n)，支持 join/split | `range_invalidation_guarantee` |
| `ov_tree_tag` | Ordered Vector | sorted vector，cache 友好 | `basic_invalidation_guarantee` |

**Node_Update 策略（节点增强）**：

| 策略 | 功能 | 新增 API |
|------|------|----------|
| `null_node_update` | 默认，无额外开销 | — |
| `tree_order_statistics_node_update` | 维护子树 size | `find_by_order(k)`, `order_of_key(x)` |

```cpp
// 顺序统计树（第 k 小）
tree<int, null_type, less<int>, rb_tree_tag,
     tree_order_statistics_node_update> ost;
ost.insert(3); ost.insert(1); ost.insert(4);
auto it = ost.find_by_order(1);     // 第 2 小 = 3
size_t rank = ost.order_of_key(2);  // < 2 的个数 = 1
```

#### 1.1.2 Trie 容器 `trie<K, V, ATraits, Tag, Node_Update, Alloc>`

| 模板参数 | 默认值 | 说明 |
|----------|--------|------|
| `Key` | — | 键类型（通常是 string） |
| `Mapped` | — | `null_type` = set |
| `_ATraits` | `default_trie_access_traits<Key>` | **元素访问特征** |
| `Tag` | `pat_trie_tag` | 唯一选择：PATRICIA trie |
| `Node_Update` | `null_node_update` | 节点更新策略 |
| `_Alloc` | `std::allocator<char>` | 分配器 |

**Trie 特有策略**：

| 策略 | 功能 |
|------|------|
| `trie_string_access_traits<String, Min, Max, Reverse>` | 字符串元素访问 |
| `trie_order_statistics_node_update` | 顺序统计（同 tree） |
| `prefix_search_node_update` | 前缀搜索支持 |
| `sample_trie_access_traits` | 自定义元素访问示例 |

**Trie 特有 API**：

```cpp
trie<string, null_type, trie_string_access_traits<>> t;
t.insert("hello"); t.insert("world"); t.insert("help");
// 前缀搜索：返回 ["hello", "help"] 的范围
auto [first, last] = t.prefix_range("hel");
```

#### 1.1.3 哈希表家族

**A. 链地址法 `cc_hash_table<K, V, Hash, Eq, Comb_Hash, Resize, Store_Hash, Alloc>`**

| 模板参数 | 默认值 | 说明 |
|----------|--------|------|
| `Key` | — | 键类型 |
| `Mapped` | — | `null_type` = set |
| `Hash_Fn` | `std::tr1::hash<Key>` | 哈希函数 |
| `Eq_Fn` | `std::equal_to<Key>` | 等价比较 |
| `Comb_Hash_Fn` | `direct_mask_range_hashing<>` | **范围哈希策略** |
| `Resize_Policy` | `hash_standard_resize_policy<...>` | **扩容策略** |
| `Store_Hash` | `false` | 是否在节点缓存 hash 值 |
| `_Alloc` | `std::allocator<char>` | 分配器 |

**范围哈希策略（Comb_Hash_Fn）**：

| 策略 | 算法 | 适用场景 |
|------|------|----------|
| `direct_mask_range_hashing<>` | `hash & (size-1)` | 2 的幂次容量，位运算快 |
| `direct_mod_range_hashing<>` | `hash % size` | 任意容量 |

**扩容策略组合（Resize_Policy = `hash_standard_resize_policy<Size_Policy, Trigger>`）**：

| 尺寸策略（Size_Policy） | 新容量计算 |
|------------------------|------------|
| `hash_exponential_size_policy` | `new_size = old_size * 2` |
| `hash_prime_size_policy` | 下一个质数 |

| 触发器（Trigger） | 扩容时机 |
|------------------|----------|
| `hash_load_check_resize_trigger<External_Load_Access>` | load factor 超出 [min, max] |
| `hash_load_check_resize_trigger_size_base` | 同上，带 size 缓存 |

**B. 开放寻址法 `gp_hash_table<K, V, Hash, Eq, Comb_Probe, Probe, Resize, Store_Hash, Alloc>`**

比 `cc_hash_table` 多两个参数：

| 模板参数 | 默认值 | 说明 |
|----------|--------|------|
| `Comb_Probe_Fn` | `direct_mask_range_hashing<>` | 探测范围哈希 |
| `Probe_Fn` | `linear_probe_fn<>` / `quadratic_probe_fn<>` | **探测序列策略** |

**探测序列策略**：

| 策略 | 公式 | 特点 |
|------|------|------|
| `linear_probe_fn` | `h(k) + i` | 简单，但主聚集 |
| `quadratic_probe_fn` | `h(k) + i²` | 减少主聚集 |

#### 1.1.4 列表更新容器 `list_update<K, V, Eq, Update_Policy, Alloc>`

| 模板参数 | 默认值 | 说明 |
|----------|--------|------|
| `Key` | — | 键类型 |
| `Mapped` | — | `null_type` = set |
| `Eq_Fn` | `std::equal_to<Key>` | 等价比较 |
| `Update_Policy` | `lu_move_to_front_policy<>` | **更新策略** |
| `_Alloc` | `std::allocator<char>` | 分配器 |

**更新策略**：

| 策略 | 算法 | 元数据 |
|------|------|--------|
| `lu_move_to_front_policy` | 每次访问移到队首 | 无（`null_type`） |
| `lu_counter_policy<Max_Count>` | 访问 Max_Count 次后移到队首 | 计数器 |

这是**在线竞争算法**的应用：自组织列表，利用访问局部性。

```cpp
list_update<int, null_type, equal_to<int>,
            lu_counter_policy<5>> lu;
// 访问 5 次后元素自动移到前面
```

---

### 1.2 优先队列（`priority_queue.hpp`）

| 模板参数 | 默认值 | 说明 |
|----------|--------|------|
| `_Tv` | — | 值类型 |
| `Cmp_Fn` | `std::less<_Tv>` | 比较器 |
| `Tag` | `pairing_heap_tag` | **堆家族选择** |
| `_Alloc` | `std::allocator<char>` | 分配器 |

**堆家族选择**：

| Tag | 底层结构 | push | pop | join | 特点 |
|-----|----------|------|-----|------|------|
| `pairing_heap_tag` | Pairing Heap | O(1) | O(log n) amortized | O(1) | 默认，综合最优 |
| `binomial_heap_tag` | 二项堆 | O(1) | O(log n) | O(log n) | 理论优雅 |
| `rc_binomial_heap_tag` | 冗余计数二项堆 | O(1) | O(log n) | O(1) | 改进的 join |
| `binary_heap_tag` | 二叉堆（数组） | O(1) amortized | O(log n) | ❌ 不支持 | 最紧凑 |
| `thin_heap_tag` | Thin Heap | O(1) | O(log n) | ❌ 不支持 | Dijkstra 最优 |

**关键差异**：只有 `pairing_heap_tag` 和 `rc_binomial_heap_tag` 支持 `join()`。

```cpp
priority_queue<int, less<int>, pairing_heap_tag> pq1, pq2;
pq1.join(pq2);  // O(1)，合并两个堆
```

---

### 1.3 PBDS 的无效化保证体系

PBDS 定义了三种迭代器/指针/引用无效化保证：

| 保证级别 | 继承关系 | 含义 |
|----------|----------|------|
| `basic_invalidation_guarantee` | 基类 | 容器不修改时，所有迭代器/指针/引用有效 |
| `point_invalidation_guarantee` | 继承上者 | 元素未被删除时，指向它的迭代器/指针/引用有效 |
| `range_invalidation_guarantee` | 继承上者 | 元素未被删除时，范围迭代器的相对位置正确 |

**各容器的保证**：

| 容器 | 保证级别 |
|------|----------|
| `tree` (RB/Splay) | `range_invalidation_guarantee` |
| `trie` | `range_invalidation_guarantee` |
| `ov_tree_tag` 的 `tree` | `basic_invalidation_guarantee`（vector 底层） |
| `cc_hash_table` / `gp_hash_table` | `point_invalidation_guarantee` |
| `list_update` | `basic_invalidation_guarantee` |
| `priority_queue` | 无迭代器，只有 `point_iterator` |

---

## 二、字符串扩展

### 2.1 `rope` — 重型平衡树字符串

```cpp
#include <ext/rope>
__gnu_cxx::rope<char> r;

r.insert(pos, "hello");     // O(log n)
r.erase(pos, count);        // O(log n)
r.substr(pos, count);       // O(log n)
r += other_rope;            // O(log n)，引用计数共享
```

**底层**：平衡二叉树，每个节点存一个字符串片段（类似 finger tree）。

**特点**：
- 拷贝是 O(1)（引用计数）
- 插入/删除/子串都是 O(log n)
- 3000+ 行代码

**适用场景**：文本编辑器、版本控制系统、DNA 序列处理。

### 2.2 `vstring` — 可变实现字符串

```cpp
#include <ext/vstring.h>
__gnu_cxx::__vstring s;  // 内部实验类型
```

**底层**：运行时选择 SSO / 引用计数 / 堆分配策略。

### 2.3 字符串基类

| 头文件 | 用途 |
|--------|------|
| `rc_string_base.h` | 引用计数字符串基类 |
| `sso_string_base.h` | SSO 字符串基类 |

这些是 `std::string` 的内部实现组件，不直接对外使用。

---

## 三、分配器家族（`<ext/*_allocator.h>`）

| 分配器 | 策略 | 特点 |
|--------|------|------|
| `new_allocator` | `::operator new` / `::operator delete` | 最基础 |
| `malloc_allocator` | `malloc` / `free` | C 风格 |
| `pool_allocator` | 内存池 | 减少碎片，复用小块 |
| `bitmap_allocator` | 位图管理 | 精确跟踪空闲块 |
| `mt_allocator` | 多线程安全 | 线程本地缓存 |
| `debug_allocator` | 包装器 | 边界检查、泄漏检测 |
| `throw_allocator` | 包装器 | **随机抛出异常**（测试用！） |
| `extptr_allocator` | 扩展指针 | 支持特殊指针类型 |

**`throw_allocator`** 是一个被低估的宝藏：

```cpp
// 随机在分配时抛出异常，测试容器的异常安全性
__gnu_cxx::throw_allocator<int> alloc;
vector<int, decltype(alloc)> v(alloc);
v.push_back(1);  // 可能抛出！
```

---

## 四、并发基础

| 头文件 | 内容 |
|--------|------|
| `concurrence.h` | `__mutex`, `__scoped_lock` — 简单互斥锁包装 |
| `atomicity.h` | `__atomic_add`, `__exchange_and_add` — 原子操作 |

这些是 libstdc++ **内部使用**的并发原语，不是公开 API。

---

## 五、其他工具头

| 头文件 | 用途 | 对 chaistl 的启示 |
|--------|------|-------------------|
| `aligned_buffer.h` | 对齐缓冲区（placement new） | 节点存储的底层工具 |
| `pointer.h` | 扩展指针工具 | 自定义指针类型支持 |
| `typelist.h` | 类型列表（MPL 前身） | PBDS 的策略类型列表用此实现 |
| `numeric_traits.h` | 数值类型特性 | 编译期数值计算 |
| `cast.h` | 安全转换工具 | — |
| `type_traits.h` | 扩展类型特性 | C++11 `<type_traits>` 的前身 |
| `stdio_filebuf.h` | FILE* → streambuf 包装 | 和容器无关 |
| `string_conversions.h` | 字符串转换辅助 | — |

---

## 六、完整容器/数据结构清单

### PBDS 内

| 类别 | 容器/数据结构 | 策略参数数量 |
|------|--------------|-------------|
| **树** | `tree` (RB/Splay/OV) | 6 (含 Tag + Node_Update) |
| **Trie** | `trie` (PATRICIA) | 6 (含 ATraits + Node_Update) |
| **哈希表** | `cc_hash_table` | 8 (最复杂) |
| **哈希表** | `gp_hash_table` | 9 (更复杂，多探测策略) |
| **自组织列表** | `list_update` | 5 |
| **优先队列** | `priority_queue` (5 种堆) | 4 |

### `<ext/>` 其他

| 类别 | 数据结构 | 说明 |
|------|----------|------|
| **字符串** | `rope` | 平衡树字符串 |
| **字符串** | `vstring` | 可变策略字符串 |
| **分配器** | 8 种 | 见第三节 |

**总计**：PBDS 内 6 个容器模板 × 多种策略组合 = **数十种有效数据结构变体**，
加上 `<ext/>` 其他约 **10+ 个独立数据结构**。

---

## 七、对 chaistl 的启示矩阵

| GCC ext 特性 | chaistl 可以借鉴 | 优先级 |
|-------------|-----------------|--------|
| PBDS `tree` 的 Tag 分发 | 已超越：concept 更现代 | — |
| PBDS `Node_Update` 节点增强 | **WBT 的 size 字段 + order statistics** | 🔥 高 |
| PBDS hash 策略分解 | **unordered_map 的哈希策略设计** | 🔥 高 |
| PBDS `Store_Hash` 布尔策略 | hash 节点缓存开关 | 中 |
| PBDS 5 种堆家族 | **priority_queue 的堆策略** | 🔥 高 |
| PBDS `list_update` | 自组织列表实验容器 | 低 |
| PBDS Trie + 前缀搜索 | 字符串专用容器 | 低 |
| PBDS 无效化保证体系 | 文档化迭代器保证 | 中 |
| `throw_allocator` | **异常安全测试基础设施** | 🔥 高 |
| `rope` | 重型字符串实验容器 | 中 |
| `pool_allocator` / `bitmap_allocator` | 分配器策略库 | 中 |

---

## 八、关键代码参考路径

```
/usr/include/c++/14/ext/
├── pb_ds/
│   ├── assoc_container.hpp          # tree, trie, cc_hash_table, gp_hash_table, list_update
│   ├── priority_queue.hpp           # priority_queue (5 种堆)
│   ├── tree_policy.hpp              # tree_order_statistics_node_update
│   ├── trie_policy.hpp              # trie_string_access_traits, prefix_search
│   ├── hash_policy.hpp              # range_hash, probe_fn, resize_trigger
│   ├── list_update_policy.hpp       # lu_move_to_front, lu_counter
│   └── tag_and_trait.hpp            # 所有 tag 定义 + 无效化保证
├── rope                             # 重型字符串
├── ropeimpl.h                       # rope 实现
├── *_allocator.h (×8)              # 分配器家族
└── concurrence.h / atomicity.h      # 并发原语
```
