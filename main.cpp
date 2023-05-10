#include <clocale>
#include <codecvt>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

/**
 * In big project srand can be done in a lot of places so it had better to have an alternative.
 */
#define SPEEDUP_GENERATOR_BY_DEDICATED_CALL_OF_SEED_RANDOM 0

template<typename Container>
using IteratorCategoryOf =
  typename std::iterator_traits<typename Container::iterator>::iterator_category;

template<typename Container>
using HaveRandomAccessIterator = std::is_base_of<
  std::random_access_iterator_tag,
  IteratorCategoryOf<Container>>;

/**
 * Base implementation of class which will be reused in the helpers below.
 * @tparam TChar character can be different.
 */
template<typename TChar>
class RandomStringGeneratorBase
{
public:
  /**
   * Very efficient constructor which will almost do nothing, there is not any virtual tables, there is not any
   * calculations in the body. May be it had better to add noexcept(I have added but should be checked if it will be
   * available for all cases.
   * @note This will be done more rarely than actual generate, and everything will be here like a configuration, to
   * avoid do this every place.
   * @param charset
   * @param charsetSize
   * @param seed provide any your own
   */
  RandomStringGeneratorBase(
    std::basic_string<TChar> charset,
    std::function<void()> seed = []() { srand(time(nullptr)); },
    std::function<size_t(size_t)> rand = [](size_t range) { return std::rand() % range; }) noexcept
      : _charset{std::move(charset)}, _seed{std::move(seed)}, _rand{std::move(rand)}
  {
#if !SPEEDUP_GENERATOR_BY_DEDICATED_CALL_OF_SEED_RANDOM
    // A lot of people thought that atomic very fast. In computers with 128 cores implementation which uses atomics
    // can slow down compared to 32 cores etc. The problem is in the nature of atomic cpu core hardware implementation.
    // So I have just shown that this is not a good idea but, it is helpful do not care about it some time.
    static std::once_flag flag;
    std::call_once(flag, [&]() {
      // this every time passed by copy, so I do not see any problems to use & to avoid exercises around changing this
      // in the future. The main fail of interviewers that = means everything copy from scope(only that one which was
      // used in the lambda), and & everything will be copied by reference(it is not true, because this will be copied by value)
      Seed();
    });
#endif
  }

  /**
   * Helper for usage arrays and avoid doing size calculation on the client code side.
   * @note due to C++ language allow to init an arrays with const char sequence pointer,
   * you should understand that \0 will be in the end, and C++ can not care about it,
   * so use for this purpose constructor which gets TChar const* instead.
   * @param charsetArray
   * @param seed
   * @param rand
   */
  template<size_t size>
  RandomStringGeneratorBase(
    TChar const (&charsetArray)[size],
    std::function<void()> seed = []() { srand(time(nullptr)); },
    std::function<size_t(size_t)> rand = [](size_t range) { return std::rand() % range; })
      : RandomStringGeneratorBase<TChar>(std::basic_string<TChar>(charsetArray, charsetArray + size),
                                         std::move(seed),
                                         std::move(rand))
  {
  }

  /**
   * Helper to use const strings.
   * @tparam size
   * @param charsetArray
   * @param seed
   * @param rand
   */
  RandomStringGeneratorBase(
    TChar const *charsetString,
    std::function<void()> seed = []() { srand(time(nullptr)); },
    std::function<size_t(size_t)> rand = [](size_t range) { return std::rand() % range; })
      : RandomStringGeneratorBase<TChar>(std::basic_string<TChar>(charsetString),
                                          std::move(seed),
                                          std::move(rand))
  {
  }

  /**
   * Generic helper constructor for any kind of random access iterator containers.
   * @tparam Container it can be std::vector or std::string for example
   * @param charset
   * @param seed
   * @param rand
   */
  template<typename Container,
           typename std::enable_if<HaveRandomAccessIterator<Container>::value>::type * = nullptr>
  RandomStringGeneratorBase(
    Container charset,
    std::function<void()> seed = []() { srand(time(nullptr)); },
    std::function<size_t(size_t)> rand = [](size_t range) { return std::rand() % range; })
      : RandomStringGeneratorBase<TChar>(std::basic_string<TChar>(charset.data(), charset.data() + charset.size()),
                                         std::move(seed),
                                         std::move(rand))
  {
  }

  /**
   * Helper for returning result in some container like vector or string.
   * @tparam T
   * @param outSize
   * @return
   */
  template<typename T>
  auto get(size_t outSize) -> T
  {
    T result(outSize, {});
    get(result.data(), result.size());
    return result;
  }

  /**
   * Very efficient implementation but not very safe due to some preconditions should be checked on the client code side.
   * @tparam TChar
   * @param out
   * @param outSize
   */
  void get(TChar *out, size_t outSize)
  {
    // This loop will be optimized, so should not be used any handwritten pointer tricks...
    for (int i = 0; i < outSize; i++) {
      out[i] = _charset[_rand(_charset.size())];
    }
  }

  /**
   * Manual asking seeding.
   */
  void Seed()
  {
    _seed();
  }

private:
  std::basic_string<TChar> _charset;
  std::function<void()> _seed;
  std::function<size_t(size_t)> _rand;
};

using RandomStringGenerator = RandomStringGeneratorBase<char>;
using RandomStringGeneratorW = RandomStringGeneratorBase<wchar_t>;
// ... etc

int main()
{
  {
    std::cout << "The simpliest and fastest usage,\n"
                 "No additional allocations for result"
              << std::endl;
    std::string outString(10, ' ');
    auto myBaseGenerator = RandomStringGenerator("0123456789abcdefghijklmnopqrstuvwxyz");

    for (auto i = 0; i < 10; i++) {
      // DUE to c++ 17 standart allow to write to std::string by data(),
      // since implementation of allocating chars now should be consequently.
      myBaseGenerator.get(outString.data(), outString.size());
      std::cout << outString << std::endl;
    }
    std::cout << std::endl;
  }

  {
    std::cout << "The simpliest and fastest usage, with an array,\n"
                 "No additional allocations for result"
              << std::endl;
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string outString(10, ' ');
    // -1 due to last symbol it is null be careful
    auto myBaseGenerator = RandomStringGenerator(charset);

    for (auto i = 0; i < 10; i++) {
      // DUE to c++ 17 standart allow to write to std::string by data(),
      // since implementation of allocating chars now should be consequently.
      myBaseGenerator.get(outString.data(), outString.size());
      std::cout << outString << std::endl;
    }
    std::cout << std::endl;
  }

  {
    std::cout << "The simplest usage with an array without tricks" << std::endl;
    const char charset[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                            'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                            'u', 'v', 'w', 'x', 'y', 'z'};
    auto myGenerator = RandomStringGenerator(charset);

    for (auto i = 0; i < 10; i++) {
      std::cout << myGenerator.get<std::string>(i + 1) << std::endl;
    }
    std::cout << std::endl;
  }

  {
    std::cout << "Usage std::string as a char set and getting result to std::string." << std::endl;
    auto charset = std::string("0123456789abcdefghijklmnopqrstuvwxyz");
    // Remember RandomStringGenerator does not own charset only points on it
    auto myGenerator = RandomStringGenerator(charset);
    for (auto i = 0; i < 10; i++) {
      std::cout << myGenerator.get<std::string>(i + 1) << std::endl;
    }
    std::cout << std::endl;
  }

  {
    std::cout << "Usage std::vector instead of std::string for char set and for results." << std::endl;
    auto charset = std::vector<char>{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                     'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                                     'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                                     'u', 'v', 'w', 'x', 'y', 'z'};
    auto myGenerator = RandomStringGenerator(charset);
    for (auto i = 0; i < 10; i++) {
      auto result = myGenerator.get<std::vector<char>>(i + 1);
      auto resultString = std::string(result.cbegin(), result.cend());
      std::cout << resultString << std::endl;
    }
    std::cout << std::endl;
  }

  setlocale(LC_CTYPE, "");
  using convert_type = std::codecvt_utf8<wchar_t>;
  std::wstring_convert<convert_type, wchar_t> converter;
  {
    std::cout << "Usage std::wstring as a char set and getting result to std::string." << std::endl;
    // Remember RandomStringGenerator does not own charset only points on it
    auto myGenerator = RandomStringGeneratorW(L"0123456789абвгдеёжзийклмнопрстуфхцчшьщъыэюя");
    for (auto i = 0; i < 10; i++) {
      auto resultString = converter.to_bytes(myGenerator.get<std::wstring>(i + 1));
      std::cout << resultString << std::endl;
    }
    std::cout << std::endl;
  }

  return 0;
}
