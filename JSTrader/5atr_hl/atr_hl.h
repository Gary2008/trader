#ifndef ATR_HL_H
#define ATR_HL_H
#ifdef  ATR_HL_EXPORTS
#define ATR_HL_API __declspec(dllexport)
#else
#define ATR_HL_API __declspec(dllimport)
#endif
#include"StrategyTemplate.h"
extern "C" ATR_HL_API StrategyTemplate * CreateStrategy(CTAAPI *ctamanager);//创建策略
extern "C" ATR_HL_API int ReleaseStrategy();//释放策略
std::vector<StrategyTemplate*>g_atr_hl_v;//全局指针

#include<vector>
#include<algorithm>
#include<regex>
#include"talib.h"
#include"libbson-1.0\bson.h"
#include"libmongoc-1.0\mongoc.h"
class ATR_HL_API atr_hl : public StrategyTemplate
{
public:
	atr_hl(CTAAPI *ctamanager);
	~atr_hl();

	void onInit();
	//TICK
	void onTick(TickData Tick);
	//BAR
	void onBar(BarData Bar);
	void on5Bar(BarData Bar);
	//报单回调
	void onOrder(std::shared_ptr<Event_Order>e);
	//成交回调
	void onTrade(std::shared_ptr<Event_Trade>e);

	//参数
	double unit_volume;


	//指标
	std::mutex m_HLCmtx;
	std::vector<double>close_vector;
	std::vector<double>high_vector;
	std::vector<double>low_vector;

	std::vector<double>close_50_vector;

	double atrget;
	double dayStart;
	double PP;
	double UP;
	double DOWN;
	double myPrice;

	double openPrice;


	std::set<std::string> m_ninetoeleven;
	std::set<std::string> m_ninetohalfeleven;
	std::set<std::string> m_ninetoone;
	std::set<std::string> m_ninetohalftwo;
	BarData m_5Bar;
	//缓存用
	double lastprice;				//更新界面
	//更新界面
	void putEvent();
};
#endif