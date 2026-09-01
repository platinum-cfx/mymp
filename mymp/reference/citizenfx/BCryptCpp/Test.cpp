#include "BCrypt.h"
#include <iostream>
#include <locale>

struct TestEntry
{
    const char* plain;
    const char* salt;
    const char* expected;
};

TestEntry testHash[] =
{
    { "",                                   "$2a$06$DCq7YPn5Rq63x1Lad4cll.",    "$2a$06$DCq7YPn5Rq63x1Lad4cll.TV4S6ytwfsfvkgY8jIucDrjc8deX1s." },
    { "",                                   "$2a$08$HqWuK6/Ng6sg9gQzbLrgb.",    "$2a$08$HqWuK6/Ng6sg9gQzbLrgb.Tl.ZHfXLhvt/SgVyWhQqgqcZ7ZuUtye" },
    { "",                                   "$2a$10$k1wbIrmNyFAPwPVPSVa/ze",    "$2a$10$k1wbIrmNyFAPwPVPSVa/zecw2BCEnBwVS2GbrmgzxFUOqW9dk4TCW" },
    { "",                                   "$2a$12$k42ZFHFWqBp3vWli.nIn8u",    "$2a$12$k42ZFHFWqBp3vWli.nIn8uYyIkbvYRvodzbfbK18SSsY.CsIQPlxO" },
    { "a",                                  "$2a$06$m0CrhHm10qJ3lXRY.5zDGO",    "$2a$06$m0CrhHm10qJ3lXRY.5zDGO3rS2KdeeWLuGmsfGlMfOxih58VYVfxe" },
    { "a",                                  "$2a$08$cfcvVd2aQ8CMvoMpP2EBfe",    "$2a$08$cfcvVd2aQ8CMvoMpP2EBfeodLEkkFJ9umNEfPD18.hUF62qqlC/V." },
    { "a",                                  "$2a$10$k87L/MF28Q673VKh8/cPi.",    "$2a$10$k87L/MF28Q673VKh8/cPi.SUl7MU/rWuSiIDDFayrKk/1tBsSQu4u" },
    { "a",                                  "$2a$12$8NJH3LsPrANStV6XtBakCe",    "$2a$12$8NJH3LsPrANStV6XtBakCez0cKHXVxmvxIlcz785vxAIZrihHZpeS" },
    { "abc",                                "$2a$06$If6bvum7DFjUnE9p2uDeDu",    "$2a$06$If6bvum7DFjUnE9p2uDeDu0YHzrHM6tf.iqN8.yx.jNN1ILEf7h0i" },
    { "abc",                                "$2a$08$Ro0CUfOqk6cXEKf3dyaM7O",    "$2a$08$Ro0CUfOqk6cXEKf3dyaM7OhSCvnwM9s4wIX9JeLapehKK5YdLxKcm" },
    { "abc",                                "$2a$10$WvvTPHKwdBJ3uk0Z37EMR.",    "$2a$10$WvvTPHKwdBJ3uk0Z37EMR.hLA2W6N9AEBhEgrAOljy2Ae5MtaSIUi" },
    { "abc",                                "$2a$12$EXRkfkdmXn2gzds2SSitu.",    "$2a$12$EXRkfkdmXn2gzds2SSitu.MW9.gAVqa9eLS1//RYtYCmB1eLHg.9q" },
    { "abcdefghijklmnopqrstuvwxyz",         "$2a$06$.rCVZVOThsIa97pEDOxvGu",    "$2a$06$.rCVZVOThsIa97pEDOxvGuRRgzG64bvtJ0938xuqzv18d3ZpQhstC" },
    { "abcdefghijklmnopqrstuvwxyz",         "$2a$08$aTsUwsyowQuzRrDqFflhge",    "$2a$08$aTsUwsyowQuzRrDqFflhgekJ8d9/7Z3GV3UcgvzQW3J5zMyrTvlz." },
    { "abcdefghijklmnopqrstuvwxyz",         "$2a$10$fVH8e28OQRj9tqiDXs1e1u",    "$2a$10$fVH8e28OQRj9tqiDXs1e1uxpsjN0c7II7YPKXua2NAKYvM6iQk7dq" },
    { "abcdefghijklmnopqrstuvwxyz",         "$2a$12$D4G5f18o7aMMfwasBL7Gpu",    "$2a$12$D4G5f18o7aMMfwasBL7GpuQWuP3pkrZrOAnqP.bmezbMng.QwJ/pG" },
    { "~!@#$%^&*()      ~!@#$%^&*()PNBFRD", "$2a$06$fPIsBO8qRqkjj273rfaOI.",    "$2a$06$fPIsBO8qRqkjj273rfaOI.HtSV9jLDpTbZn782DC6/t7qT67P6FfO" },
    { "~!@#$%^&*()      ~!@#$%^&*()PNBFRD", "$2a$08$Eq2r4G/76Wv39MzSX262hu",    "$2a$08$Eq2r4G/76Wv39MzSX262huzPz612MZiYHVUJe/OcOql2jo4.9UxTW" },
    { "~!@#$%^&*()      ~!@#$%^&*()PNBFRD", "$2a$10$LgfYWkbzEvQ4JakH7rOvHe",    "$2a$10$LgfYWkbzEvQ4JakH7rOvHe0y8pHKF9OaFgwUZ2q7W2FFZmZzJYlfS" },
    { "~!@#$%^&*()      ~!@#$%^&*()PNBFRD", "$2a$12$WApznUOJfkEGSmYRfnkrPO",    "$2a$12$WApznUOJfkEGSmYRfnkrPOr466oFDCaj4b6HY3EXGvfxm43seyhgC" },
};

#define NumLineOf(array) sizeof(array)/sizeof(array[0])

void TestHashPassword()
{
    // test hash password
    std::cout << "BCrypt::HashPassword(): " << std::endl;
    for (auto i = 0; i < NumLineOf(testHash); ++i)
    {
        std::string plain = testHash[i].plain;
        std::string salt = testHash[i].salt;
        std::string hashed = BCryptCpp::BCrypt::HashPassword(plain, salt);
        std::string expected = testHash[i].expected;

        std::cout << "plain    : " << plain << std::endl;
        std::cout << "salt     : " << salt << std::endl;
        std::cout << "expected : " << expected << std::endl;
        std::cout << "hashed   : " << hashed << std::endl;
        std::cout << "state    : " << (expected == hashed ? "matched" : "mismatch!") << "\n" << std::endl;
    }
}

void TestGenerateSaltWorkfactor()
{
    std::cout << "BCrypt::GenerateSalt(log_rounds): " << std::endl;

    for (auto i = 4; i < 12; ++i)
    {
        std::cout << "work factor = " << i << ": " << std::endl;
        for (auto j = 0; j < NumLineOf(testHash); ++j)
        {
            std::string plain = testHash[j].plain;
            std::string salt = BCryptCpp::BCrypt::GenerateSalt(i);
            std::string hashed1 = BCryptCpp::BCrypt::HashPassword(plain, salt);
            std::string hashed2 = BCryptCpp::BCrypt::HashPassword(plain, hashed1);
            std::cout << "plain    : " << plain << std::endl;
            std::cout << "salt     : " << salt << std::endl;
            std::cout << "hashed1  : " << hashed1 << std::endl;
            std::cout << "hashed2  : " << hashed2 << std::endl;
            std::cout << "state    : " << (hashed1 == hashed2 ? "equal" : "not equal") << "\n" << std::endl;
        }
    }
}

void TestGenerateSalt()
{
    std::cout << "BCrypt::GenerateSalt(): " << std::endl;

    for (auto i = 0; i < NumLineOf(testHash); ++i)
    {
        std::string plain = testHash[i].plain;
        std::string salt = BCryptCpp::BCrypt::GenerateSalt();
        std::string hashed1 = BCryptCpp::BCrypt::HashPassword(plain, salt);
        std::string hashed2 = BCryptCpp::BCrypt::HashPassword(plain, hashed1);
        std::cout << "plain    : " << plain << std::endl;
        std::cout << "salt     : " << salt << std::endl;
        std::cout << "hashed1  : " << hashed1 << std::endl;
        std::cout << "hashed2  : " << hashed2 << std::endl;
        std::cout << "state    : " << (hashed1 == hashed2 ? "equal" : "not equal") << "\n" << std::endl;
    }
}

void TestCheckPasswordSuccess()
{
    std::cout << "BCrypt::checkpw(String, String) with good password: " << std::endl;
    for (auto i = 0; i < NumLineOf(testHash); ++i)
    {
        std::string plain = testHash[i].plain;
        std::string expected = testHash[i].expected;
        bool bValue = BCryptCpp::BCrypt::CheckPassword(plain, expected);
        std::cout << "plain    : " << plain << std::endl;
        std::cout << "expected : " << expected << std::endl;
        std::cout << "hashed   : " << BCryptCpp::BCrypt::HashPassword(plain, expected) << std::endl;
        std::cout << "IsTrue   : " << (bValue ? "True" : "False") << "\n" << std::endl;
    }
}

void TestCheckPasswordFailure()
{
    std::cout << "BCrypt::checkpw(String, String) with bad password: " << std::endl;
    for (auto i = 0; i < NumLineOf(testHash); ++i)
    {
        int broken_index = (i + 4) % NumLineOf(testHash);
        std::string plain = testHash[i].plain;
        std::string expected = testHash[broken_index].expected;
        bool bValue = BCryptCpp::BCrypt::CheckPassword(plain, expected);
        std::cout << "plain    : " << plain << std::endl;
        std::cout << "expected : " << expected << std::endl;
        std::cout << "hashed   : " << BCryptCpp::BCrypt::HashPassword(plain, expected) << std::endl;
        std::cout << "IsTrue   : " << (bValue ? "True" : "False") << "\n" << std::endl;
    }
}

#include <codecvt>
namespace StringConvert
{
std::string ws2s(const std::wstring& wstr)
{
    typedef std::codecvt_utf8<wchar_t> convert_typeX;
    std::wstring_convert<convert_typeX, wchar_t> ConverterX;
    return ConverterX.to_bytes(wstr);
}
std::wstring s2ws(const std::string& str)
{
    typedef std::codecvt_utf8<wchar_t> convert_typeX;
    std::wstring_convert<convert_typeX, wchar_t> ConverterX;
    return ConverterX.from_bytes(str);
}
}

// Be very careful the wide string and string will give different result!
void TestInternationalCharacter()
{
    std::cout << "BCrypt::HashPassword() with international character: " << std::endl;
    std::wstring pwU = L"\u2605\u2605\u2605\u2605\u2605\u2605\u2605\u2605";
    std::string pw1 = StringConvert::ws2s(pwU);
    std::string pw2 = "????????";
    std::string h1 = BCryptCpp::BCrypt::HashPassword(pw1);
    std::cout << "real hashed  : " << h1 << std::endl;
    bool bValue = BCryptCpp::BCrypt::CheckPassword(pw2, h1);
    std::cout << "input hashed : " << BCryptCpp::BCrypt::HashPassword(pw2, h1) << std::endl;
    std::cout << "IsTrue       : " << (bValue ? "True" : "False") << "\n" << std::endl;

    std::string h2 = BCryptCpp::BCrypt::HashPassword(pw2);
    std::cout << "real hashed  : " << h2 << std::endl;
    bValue = BCryptCpp::BCrypt::CheckPassword(pw1, h2);
    std::cout << "input hashed : " << BCryptCpp::BCrypt::HashPassword(pw1, h2) << std::endl;
    std::cout << "IsTrue       : " << (bValue ? "True" : "False") << "\n" << std::endl;
}

int main()
{
    TestHashPassword();
    TestGenerateSaltWorkfactor();
    TestGenerateSalt();
    TestCheckPasswordSuccess();
    TestCheckPasswordFailure();
    TestInternationalCharacter();
    return 0;
}