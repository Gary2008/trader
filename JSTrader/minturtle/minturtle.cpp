#include"minturtle.h"


/*
海龟交易法则：50周期通道，25周期卖出通道，每日加仓最多两次，一共加仓最多6次
*/
StrategyTemplate* CreateStrategy(CTAAPI *ctamanager)
{
	//创建策略
	StrategyTemplate *strategy = new minturtle(ctamanager);
	g_minturtle_v.push_back(strategy);
	return strategy;
}

int ReleaseStrategy()//多品种要删除多次对象
{
	//释放策略对象
	for (std::vector<StrategyTemplate*>::iterator it = g_minturtle_v.begin(); it != g_minturtle_v.end(); it++)
	{
		if ((*it) != nullptr)
		{
			//删除指针
			delete *it;
			*it = nullptr;
		}
	}
	return 0;
}


minturtle::minturtle(CTAAPI *ctamanager) :StrategyTemplate(ctamanager)
{
	//基本参数
	m_ctamanager = ctamanager;
	trademode = BAR_MODE;
	tickDbName = "CTPTickDb";
	BarDbName = "CTPMinuteDb";
	gatewayname = "CTP";
	initDays = 10;
	unitLimit = 2;
	volumelimit = 13;

	/*****************夜盘收盘平仓*************************/
	std::string ninetoeleven[] = { "bu", "rb", "hc", "ru" };//9点到11点的合约列表
	std::string ninetohalfeleven[] = { "p", "j", "m", "y", "a", "b", "jm", "i", "SR", "CF", "RM", "MA", "ZC", "FG", "OI" };//9点到11点半的合约
	std::string ninetoone[] = { "cu", "al", "zn", "pb", "sn", "ni" };//9点到1点的合约列表
	std::string ninetohalftwo[] = { "ag", "au" };//9点到2点半的合约
	for (int i = 0; i < sizeof(ninetoeleven) / sizeof(ninetoeleven[0]); ++i)
	{
		m_ninetoeleven.insert(ninetoeleven[i]);
	}
	for (int i = 0; i < sizeof(ninetohalfeleven) / sizeof(ninetohalfeleven[0]); ++i)
	{
		m_ninetohalfeleven.insert(ninetohalfeleven[i]);
	}
	for (int i = 0; i < sizeof(ninetoone) / sizeof(ninetoone[0]); ++i)
	{
		m_ninetoone.insert(ninetoone[i]);
	}
	for (int i = 0; i < sizeof(ninetohalftwo) / sizeof(ninetohalftwo[0]); ++i)
	{
		m_ninetohalftwo.insert(ninetohalftwo[i]);
	}
}
//TICK

void minturtle::onInit()
{
	StrategyTemplate::onInit();
	/*************************************************/

	putEvent();
}


void minturtle::onTick(TickData Tick)
{

	putEvent();

	int tickMinute = Tick.getminute();
	int tickHour = Tick.gethour();
	m_algorithm->checkPositions_Tick(&Tick);
	m_hourminutemtx.lock();
	if ((tickMinute != m_minute) || tickHour != m_hour)
	{
		if (m_bar.close != 0)
		{
			if (!((m_hour == 11 && m_minute == 30) || (m_hour == 15 && m_minute == 00) || (m_hour == 10 && m_minute == 15)))
			{
				onBar(m_bar);
			}
		}
		BarData bar;
		bar.symbol = Tick.symbol;
		bar.exchange = Tick.exchange;
		bar.open = Tick.lastprice;
		bar.high = Tick.lastprice;
		bar.low = Tick.lastprice;
		bar.close = Tick.lastprice;
		bar.highPrice = Tick.highPrice;
		bar.lowPrice = Tick.lowPrice;
		bar.upperLimit = Tick.upperLimit;
		bar.lowerLimit = Tick.lowerLimit;
		bar.openInterest = Tick.openInterest;
		bar.openPrice = Tick.openPrice;
		bar.volume = Tick.volume;
		bar.date = Tick.date;
		bar.time = Tick.time;
		bar.unixdatetime = Tick.unixdatetime;

		m_bar = bar;
		m_minute = tickMinute;
		m_hour = tickHour;
	}
	else
	{
		m_bar.high = std::max(m_bar.high, Tick.lastprice);
		m_bar.low = std::min(m_bar.low, Tick.lastprice);
		m_bar.close = Tick.lastprice;
		m_bar.volume = Tick.volume;
		m_bar.highPrice = Tick.highPrice;
		m_bar.lowPrice = Tick.lowPrice;
	}
	lastprice = Tick.lastprice;
	m_hourminutemtx.unlock();
	//仓位控制

}
//BAR
void minturtle::onBar(BarData Bar)
{
	if (TradingMode == BacktestMode)
	{
		m_hour = Bar.gethour();
		m_minute = Bar.getminute() + 1;
		//提前跟踪变动，以防return截断
		std::unique_lock<std::mutex>lck(m_VarPlotmtx);
		m_VarPlot["high100"] = Utils::doubletostring(high_100);
		m_VarPlot["low100"] = Utils::doubletostring(low_100);
		m_VarPlot["high10"] = Utils::doubletostring(high_10);
		m_VarPlot["low10"] = Utils::doubletostring(low_10);
		m_indicatorPlot["width"] = Utils::doubletostring(width);
		m_indicatorPlot["pos"] = Utils::doubletostring(getpos(Bar.symbol));
	}

	//计算指标

	std::unique_lock<std::mutex>lck(m_HLCmtx);
	if (high_vector.size()>100)
	{
		high_vector.erase(high_vector.begin());
		close_vector.erase(close_vector.begin());
		low_vector.erase(low_vector.begin());
	}

	if (close_10_vector.size() > 30)
	{
		close_10_vector.erase(close_10_vector.begin());
	}

	close_10_vector.push_back(Bar.close);
	close_vector.push_back(Bar.close);
	high_vector.push_back(Bar.high);
	low_vector.push_back(Bar.low);

	if (close_vector.size()<100)
	{
		return;
	}


	std::vector<double>::iterator biggest10 = std::max_element(std::begin(close_10_vector), std::end(close_10_vector));
	std::vector<double>::iterator smallest10 = std::min_element(std::begin(close_10_vector), std::end(close_10_vector));

	holdingProfit = (Bar.close - holdingPrice)*getpos(Bar.symbol)*atoi(getparam("size").c_str());
	highProfit = std::max(highProfit, holdingProfit);
	if (getpos(Bar.symbol) == 1 || getpos(Bar.symbol) == 0 || getpos(Bar.symbol) == -1)
	{
		highProfit = 0;
	}
	high_10 = *biggest10;
	low_10 = *smallest10;


	width = high_100 - low_100;
	if (!((Bar.gethour() == 9 && Bar.getminute() >= 0) && (Bar.gethour() == 9 && Bar.getminute() <= 10) ||
		(Bar.gethour() == 21 && Bar.getminute() >= 0) && (Bar.gethour() == 21 && Bar.getminute() <= 10)))
	{
		if (getpos(Bar.symbol) == 0)
		{
			if (Bar.close <= low_10 && Bar.close <= low_100)
			{
				//追空
				m_algorithm->set_supposedPos(Bar.symbol, -1);
			}
			else if (Bar.close >= high_10&&Bar.close >= high_100)
			{
				//追多
				m_algorithm->set_supposedPos(Bar.symbol, 1);
			}
		}
		else if (getpos(Bar.symbol) > 0)
		{
			//有多头
			if (Bar.close >= high_100 && Bar.close - high_100 >= 5 * atoi(getparam("delta").c_str()) &&
				getpos(Bar.symbol) < volumelimit&&width > 35 )
			{
				//追多
				m_algorithm->set_supposedPos(Bar.symbol, getpos(Bar.symbol) + 3);
			}
			else if ((Bar.close <= low_10))
			{
				//平仓
				m_algorithm->set_supposedPos(Bar.symbol, 0);
			}
		}
		else if (getpos(Bar.symbol) < 0)
		{
			//有空头
			if (Bar.close <= low_100 &&  low_100 - Bar.close >= 5 * atoi(getparam("delta").c_str()) &&
				getpos(Bar.symbol) > -volumelimit&&width > 35)
			{
				//追空
				m_algorithm->set_supposedPos(Bar.symbol, getpos(Bar.symbol) - 3);
			}
			else if ((Bar.close >= high_10))
			{
				//平仓
				m_algorithm->set_supposedPos(Bar.symbol, 0);
			}
		}
		
	}
	/*
	if (getpos(Bar.symbol) > 4)
	{
		if (Bar.close < high_100 - 25)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 1);
		}
	}
	else if (getpos(Bar.symbol) < -4)
	{
		if (Bar.close > low_100 + 25)
		{
			m_algorithm->set_supposedPos(Bar.symbol, -1);
		}
	}
		
	if (getpos(Bar.symbol) >= 10)
	{
		if (Bar.close > high_100 + 25)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 1);
		}
	}
	if (getpos(Bar.symbol) <= -10)
	{
		if (Bar.close > low_100 - 25)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 1);
		}
	}
	*/

	//防亏神器
	holdingProfit = (Bar.close - holdingPrice)*getpos(Bar.symbol)*atoi(getparam("size").c_str());
	highProfit = std::max(highProfit, holdingProfit);
	if (getpos(Bar.symbol) == 1 || getpos(Bar.symbol) == 0 || getpos(Bar.symbol) == -1)
	{
		highProfit = 0;
	}
	if (highProfit != 0)
	{
		if ((highProfit - holdingProfit) / highProfit < 0.5&&highProfit>2000)
		{
			if (getpos(Bar.symbol) > 0)
			{
				m_algorithm->set_supposedPos(Bar.symbol, 1);
			}
			else if (getpos(Bar.symbol) < 0)
			{
				m_algorithm->set_supposedPos(Bar.symbol, -1);
			}
		}
	}

	if (m_minute % 5 == 0)
	{
		if (m_5Bar.close != 0)
		{
			if (!((m_hour == 11 && m_minute == 30) || (m_hour == 15 && m_minute == 00) || (m_hour == 10 && m_minute == 15)))
			{
				on5Bar(m_5Bar);
			}
		}
		BarData bar_5min;
		bar_5min.symbol = Bar.symbol;
		bar_5min.exchange = Bar.exchange;
		bar_5min.open = Bar.open;
		bar_5min.high = Bar.high;
		bar_5min.low = Bar.low;
		bar_5min.close = Bar.close;
		bar_5min.highPrice = Bar.highPrice;
		bar_5min.lowPrice = Bar.lowPrice;
		bar_5min.upperLimit = Bar.upperLimit;
		bar_5min.lowerLimit = Bar.lowerLimit;
		bar_5min.openInterest = Bar.openInterest;
		bar_5min.openPrice = Bar.openPrice;
		bar_5min.volume = Bar.volume;
		bar_5min.date = Bar.date;
		bar_5min.time = Bar.time;
		bar_5min.unixdatetime = Bar.unixdatetime;

		m_5Bar = bar_5min;
		m_minute = m_minute;
		m_hour = m_hour;
	}
	else
	{
		m_5Bar.high = std::max(m_5Bar.high, Bar.high);
		m_5Bar.low = std::min(m_5Bar.low, Bar.low);
		m_5Bar.close = Bar.close;
		m_5Bar.volume += Bar.volume;
		m_5Bar.highPrice = Bar.highPrice;
		m_5Bar.lowPrice = Bar.lowPrice;
	}

	if (abs(getpos(Bar.symbol)) >= 4)
	{
		if (m_hour == 14 && m_minute == 58)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 0);
		}
	}

	if (Utils::getWeedDay(Bar.date) == 5)
	{
		if (m_ninetoeleven.find(Utils::regMySymbol(Bar.symbol)) != m_ninetoeleven.end())
		{
			if (m_hour == 22 && m_minute == 58)
			{
				m_algorithm->set_supposedPos(Bar.symbol, 0);

			}
		}
		else if (m_ninetohalfeleven.find(Utils::regMySymbol(Bar.symbol)) != m_ninetohalfeleven.end())
		{
			if (m_hour == 23 && m_minute == 28)
			{
				m_algorithm->set_supposedPos(Bar.symbol, 0);
			}
		}
		else if ((m_ninetohalfeleven.find(Utils::regMySymbol(Bar.symbol)) == m_ninetohalfeleven.end()) &&
			(m_ninetoeleven.find(Utils::regMySymbol(Bar.symbol)) == m_ninetoeleven.end()) &&
			(m_ninetoone.find(Utils::regMySymbol(Bar.symbol)) == m_ninetoone.end()) &&
			(m_ninetohalftwo.find(Utils::regMySymbol(Bar.symbol)) == m_ninetohalftwo.end()))
		{
			if (m_hour == 14 && m_minute == 58)
			{
				m_algorithm->set_supposedPos(Bar.symbol, 0);
			}
		}
	}
	if (Utils::getWeedDay(Bar.date) == 6)
	{
		if (m_ninetoone.find(Utils::regMySymbol(Bar.symbol)) != m_ninetoone.end())
		{
			if (m_hour == 00 && m_minute == 58)
			{
				m_algorithm->set_supposedPos(Bar.symbol, 0);
			}
		}
		else if (m_ninetohalftwo.find(Utils::regMySymbol(Bar.symbol)) != m_ninetohalftwo.end())
		{
			if (m_hour == 2 && m_minute == 28)
			{
				m_algorithm->set_supposedPos(Bar.symbol, 0);
			}
		}
	}

	if (TradingMode == BacktestMode)
	{
		m_algorithm->setTradingMode(BacktestMode);
		m_algorithm->checkPositions_Bar(&Bar);
	}
	putEvent();
}

void minturtle::on5Bar(BarData Bar)
{
	std::vector<double>::iterator biggest100 = std::max_element(std::begin(close_vector), std::end(close_vector));
	std::vector<double>::iterator smallest100 = std::min_element(std::begin(close_vector), std::end(close_vector));
	high_100 = *biggest100;
	low_100 = *smallest100;
	opened = false; 
	//回测用发单函数
	if (TradingMode == BacktestMode)
	{
		m_algorithm->setTradingMode(BacktestMode);
		m_algorithm->checkPositions_Bar(&Bar);
	}
}
//报单回调
void minturtle::onOrder(std::shared_ptr<Event_Order>e)
{


}
//成交回调
void minturtle::onTrade(std::shared_ptr<Event_Trade>e)
{
	openPrice = e->price;
	if (e->offset == "offsetopen")
	{
		holdingPrice = ((abs(getpos(e->symbol)) - e->volume)*holdingPrice + e->volume*e->price) / abs(getpos(e->symbol));
	}
	else if (e->offset == "offsetclose")
	{
		holdingPrice = e->price;
	}
}

//更新参数到界面
void minturtle::putEvent()
{
	m_strategydata->insertvar("inited", Utils::booltostring(inited));
	m_strategydata->insertvar("trading", Utils::booltostring(trading));
	//更新仓位
	std::map<std::string, double>map = getposmap();
	for (std::map<std::string, double>::iterator iter = map.begin(); iter != map.end(); iter++)
	{
		m_strategydata->insertvar(("pos_" + iter->first), Utils::doubletostring(iter->second));
	}

	m_strategydata->insertvar("lastprice", Utils::doubletostring(lastprice));

	m_strategydata->insertvar("100high", Utils::doubletostring(high_100));
	m_strategydata->insertvar("100low", Utils::doubletostring(low_100));

	m_strategydata->insertvar("10high", Utils::doubletostring(high_10));
	m_strategydata->insertvar("10low", Utils::doubletostring(low_10));

	m_strategydata->insertvar("width", Utils::doubletostring(width));
	

	//将参数和变量传递到界面上去
	std::shared_ptr<Event_UpdateStrategy>e = std::make_shared<Event_UpdateStrategy>();
	e->parammap = m_strategydata->getallparam();
	e->varmap = m_strategydata->getallvar();
	e->strategyname = m_strategydata->getparam("name");
	m_ctamanager->PutEvent(e);
}