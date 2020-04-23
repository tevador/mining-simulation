// (c) tevador 2020
// License: MIT

#include <cstdint>
#include <random>
#include <string>
#include <iostream>

using BlockReward = uint64_t;
using Height = uint_fast32_t;
using Generator = std::mt19937;

constexpr double xmrUnit = 1e12;
constexpr BlockReward moneySupply = UINT64_MAX;

constexpr BlockReward operator "" _xmr(long double xmr) {
	return (xmr * xmrUnit) + 0.5;
}

constexpr BlockReward getBaseReward(BlockReward totalSupply) {
	return (moneySupply - totalSupply) >> 18;
}

constexpr BlockReward tailEmission = 0.6_xmr;

class Pool;

class Block {
public:
	Block()
		: minedBy_(nullptr), reward_(getBaseReward(0)), height_(0)
	{
	}

	Block(const Pool* minedBy, BlockReward reward, Height height)
		:
		minedBy_(minedBy), reward_(reward), height_(height)
	{
	}

	BlockReward getReward() const {
		return reward_;
	}

private:
	const Pool* minedBy_;
	BlockReward reward_;
	Height height_;
};

class Pool {
public:
	Pool(std::string name, double hashrate)
		: name_(name), hashrate_(hashrate)
	{}

	void addBlock(const Block& block) {
		rewards_ += block.getReward();
		++blocks_;
	}

	double getHashrate() const {
		return hashrate_;
	}

	Height getBlocks() const {
		return blocks_;
	}

	BlockReward getRewards() const {
		return rewards_;
	}

private:
	std::string name_;
	double hashrate_;
	BlockReward rewards_ = 0;
	Height blocks_ = 0;
};



class StatsSet {
public:
	StatsSet(std::string name)
		: name_(name)
	{}

	void addValue(double value) {
		values_.push_back(value);
	}

	void printStats(std::ostream& os) const {
		double sum = 0.0;
		for (auto val : values_) {
			sum += val;
		}
		double mean = sum / values_.size();
		double varsum = 0.0;
		for (auto val : values_) {
			varsum += (val - mean) * (val - mean);
		}
		double var = varsum / values_.size() / values_.size();
		double stddev = sqrt(var);

		os << name_ << ": " << mean << " +/- " << stddev << std::endl;
	}

private:
	std::string name_;
	std::vector<double> values_;
};

class PoolStats {
public:
	PoolStats(std::string name, double hashrate)
		:
		name_(name), hashrate_(hashrate), blockCounts_("blocks"), blockRewards_("reward")
	{
	}

	Pool getPool() const {
		return Pool(name_, hashrate_);
	}

	void accumulate(const Pool& pool) {
		blockCounts_.addValue(pool.getBlocks());
		blockRewards_.addValue(pool.getRewards() / xmrUnit);
	}

	void printStats(std::ostream& os) const {
		os << "Pool " << name_ << std::endl;
		blockCounts_.printStats(os);
		blockRewards_.printStats(os);
	}

private:
	std::string name_;
	double hashrate_;
	StatsSet blockCounts_;
	StatsSet blockRewards_;
};

struct Network {
public:
	Network(Generator::result_type seed, Height height, BlockReward supply, std::vector<Pool>& pools)
		: 
		height_(height),
		supply_(supply),
		pools_(pools),
		rng_(seed),
		distr_(0.0, 1.0) 
	{
	}

	double getDouble() {
		return distr_(rng_);
	}

	BlockReward getBlockReward() const {
		auto reward = getBaseReward(supply_);
		if (reward < tailEmission) {
			reward = tailEmission;
		}
		return reward;
	}

	Block mineBlock() {
		Pool* pool = nullptr;
		auto pivot = getDouble();
		auto probe = 0.0;
		
		for (auto it = pools_.begin(); it != pools_.end(); ++it) {
			probe += it->getHashrate();
			if (probe >= pivot) {
				pool = &*it;
				break;
			}
		}

		Block b(pool, getBlockReward(), ++height_);
		
		supply_ += b.getReward();

		if (pool != nullptr) {
			pool->addBlock(b);
		}

		return b;
	}

private:
	Height height_;
	BlockReward supply_;
	std::vector<Pool>& pools_;
	Generator rng_;
	std::uniform_real_distribution<> distr_;
};

void simulateUntilTailEmission(std::vector<PoolStats>& stats, Generator::result_type seed, Height startingHeight, BlockReward startingSupply) {
	std::vector<Pool> pools;

	for (auto& pool : stats) {
		pools.push_back(pool.getPool());
	}

	Network net(seed, startingHeight, startingSupply, pools);

	for (Block b; b.getReward() > tailEmission; b = net.mineBlock())
	{
	}

	for (unsigned i = 0; i < stats.size(); ++i) {
		stats[i].accumulate(pools[i]);
	}
}

int main(int argc, char** argv) {	
	constexpr Height startingHeight = 2082536;
	constexpr BlockReward startingSupply = 17532973.286521961314_xmr;

	std::vector<PoolStats> pools;

	pools.push_back(PoolStats("A", 0.3));
	pools.push_back(PoolStats("B", 0.003));

	for (Generator::result_type seed = 1; seed <= 1000; ++seed) {
		simulateUntilTailEmission(pools, seed, startingHeight, startingSupply);
	}

	for (auto& pool : pools) {
		pool.printStats(std::cout);
	}

	return 0;
}
