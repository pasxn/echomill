#include "../src/instrument.hpp"
#include "../src/instrumentmanager.hpp"
#include <fstream>
#include <gtest/gtest.h>

using namespace echomill;

class InstrumentManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create temporary test file
        std::ofstream out("test_instruments.json");
        out << R"([
            {
                "symbol": "TEST",
                "tick_size": 0.01,
                "lot_size": 10,
                "price_scale": 10000,
                "description": "Test Instrument"
            }
        ])";
        out.close();
    }

    void TearDown() override { std::remove("test_instruments.json"); }

    InstrumentManager manager;
};

TEST_F(InstrumentManagerTest, LoadFromFile)
{
    manager.loadFromFile("test_instruments.json");
    EXPECT_EQ(1, manager.count());

    const auto& instr = manager.find("TEST");
    EXPECT_EQ("TEST", instr.symbol);
    EXPECT_EQ(10, instr.lotSize);
    EXPECT_EQ(100, instr.tickSize);
    EXPECT_EQ(100, instr.tickSize);
}

TEST_F(InstrumentManagerTest, FindUnknown) { EXPECT_THROW((void)manager.find("UNKNOWN"), std::runtime_error); }

TEST_F(InstrumentManagerTest, AllSymbols)
{
    manager.addInstrument({"SYM1", "Desc", 1, 1, 10000});
    manager.addInstrument({"SYM2", "Desc", 1, 1, 10000});

    auto symbols = manager.allSymbols();
    EXPECT_EQ(2, symbols.size());
    // Order undefined (unordered_map)
    bool found1 = false, found2 = false;
    for (const auto& s : symbols) {
        if (s == "SYM1")
            found1 = true;
        if (s == "SYM2")
            found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(InstrumentManagerTest, LoadInvalidFile)
{
    EXPECT_THROW(manager.loadFromFile("non_existent.json"), std::runtime_error);
}
