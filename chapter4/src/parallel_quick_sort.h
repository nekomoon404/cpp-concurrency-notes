#ifndef parallel_quick_sort_h_
#define parallel_quick_sort_h_
#include <algorithm>
#include <future>
#include <iostream>
#include <list>

#include "thread_pool.h"

// 1. 函数式编程使用快速排序, 函数式编程类似于数学中的函数
// 输入固定的情况下, 输出也是固定的, 不会改动共享状态
template <typename T>
std::list<T> sequential_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  // 将input中的第一个元素移动到result中, splice()不需要拷贝或分配内存
  result.splice(result.begin(), input, input.begin());
  // 将上面这个元素作为基准元素pivot element
  T const& pivot = *result.begin();

  // std::partition()将容器中中的元素按照输入的条件(谓词)重新排序,
  // 将满足条件(谓词返回true)的元素都排在不满足条件的元素前面
  // 它返回一个迭代器, 指向重新排序后第一个使谓词返回false的元素
  auto divide_point = std::partition(input.begin(), input.end(),
                                     [&](T const& t) { return t < pivot; });
  // 将小于pivot的元素放入lower_part中
  std::list<T> lower_part;
  lower_part.splice(lower_part.begin(), input, input.begin(), divide_point);

  // 分而治之, 将左半和右半序列分别进行排序
  auto new_lower(sequential_quick_sort(std::move(lower_part)));
  auto new_higher(sequential_quick_sort(std::move(input)));

  // 将new_higher移动到result的尾部, 将new_lower移动到result的头部
  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower);

  return result;
}

void test_sequential_sort() {
  std::list<int> nums = {6, 1, 0, 7, 5, 2, 9, -1};
  auto sort_result = sequential_quick_sort(nums);
  std::cout << "sorted result is ";
  for (const int num : sort_result) {
    std::cout << " " << num;
  }
  std::cout << std::endl;
}

// 2. 并行版的快速排序, 在上面实现的基础上使用std::async来新启一个线程
// 处理lower_part的排序, 排序1000个数大致需要10个线程, 2^10=1024;
// std::async的实现会让它在发现不能开辟新线程时, 采用串行方式处理新任务,
// 不会开辟新线程, 但这样不受开发者自己控制的开销不是好事情
template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  result.splice(result.begin(), input, input.begin());

  const T& pivot = *result.begin();

  auto divide_point = std::partition(input.begin(), input.end(),
                                     [&](T const& t) { return t < pivot; });

  std::list<T> lower_part;
  lower_part.splice(lower_part.begin(), input, input.begin(), divide_point);

  // lower_part是副本, 排序时多个线程之间不存在共享数据
  std::future<std::list<T>> new_lower =
      std::async(&parallel_quick_sort<T>, std::move(lower_part));
  // parallel_quick_sort前也可以不加取地址符

  auto new_higher(parallel_quick_sort(std::move(input)));

  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower.get());

  return result;
}

void test_parallel_sort() {
  std::list<int> nums = {6, 1, 0, 7, 5, 2, 9, -1};
  auto sort_result = parallel_quick_sort(nums);
  std::cout << "sorted result is ";
  for (const int num : sort_result) {
    std::cout << " " << num;
  }
  std::cout << std::endl;
}

// 3. 也可以利用之前实现过的线程池来实现并行版的快速排序, 使开辟的新线程数量可控
template <typename T>
std::list<T> thread_pool_quick_sort(std::list<T> input) {
  if (input.empty()) {
    return input;
  }
  std::list<T> result;
  result.splice(result.begin(), input, input.begin());

  const T& pivot = *result.begin();

  auto divide_point = std::partition(input.begin(), input.end(),
                                     [&](T const& t) { return t < pivot; });

  std::list<T> lower_part;
  lower_part.splice(lower_part.begin(), input, input.begin(), divide_point);

  // 将lower_part部分的排序提交给线程池
  auto new_lower = ThreadPool::instance().Commit(&parallel_quick_sort<T>,
                                                 std::move(lower_part));

  auto new_higher(thread_pool_quick_sort(std::move(input)));

  result.splice(result.end(), new_higher);
  result.splice(result.begin(), new_lower.get());

  return result;
}

void test_thread_pool_sort() {
  std::list<int> nums = {6, 1, 0, 7, 5, 2, 9, -1};
  auto sort_result = thread_pool_quick_sort(nums);
  std::cout << "sorted result is ";
  for (const int num : sort_result) {
    std::cout << " " << num;
  }
  std::cout << std::endl;
}

#endif