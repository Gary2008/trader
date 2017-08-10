#include"qushi_wangge.h"


/*
海龟交易法则：50周期通道，25周期卖出通道，每日加仓最多两次，一共加仓最多6次
*/
StrategyTemplate* CreateStrategy(CTAAPI *ctamanager)
{
	//创建策略
	StrategyTemplate *strategy = new qushi_wangge(ctamanager);
	g_qushi_wangge_v.push_back(strategy);
	return strategy;
}

int ReleaseStrategy()//多品种要删除多次对象
{
	//释放策略对象
	for (std::vector<StrategyTemplate*>::iterator it = g_qushi_wangge_v.begin(); it != g_qushi_wangge_v.end(); it++)
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


qushi_wangge::qushi_wangge(CTAAPI *ctamanager) :StrategyTemplate(ctamanager)
{
	//基本参数
	m_ctamanager = ctamanager;
	trademode = BAR_MODE;
	tickDbName = "CTPTickDb";
	BarDbName = "CTPMinuteDb";
	gatewayname = "CTP";
	initDays = 10;
	unitLimit = 2;
	count = 0;
	lastHour = 0;
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

void qushi_wangge::onInit()
{
	StrategyTemplate::onInit();
	/*************************************************/
	unitLimit = 2;

	putEvent();
}


void qushi_wangge::onTick(TickData Tick)
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
void qushi_wangge::onBar(BarData Bar)
{
	if (TradingMode == BacktestMode)
	{
		m_hour = Bar.gethour();
		m_minute = Bar.getminute() + 1;
		//提前跟踪变动，以防return截断
		m_VarPlotmtx.lock();
		m_VarPlot["wg1"] = Utils::doubletostring(wg1);
		m_VarPlot["wg2"] = Utils::doubletostring(wg2);
		m_VarPlot["upperborder"] = Utils::doubletostring(upperBorder);
		m_VarPlot["lowerborder"] = Utils::doubletostring(lowerBorder);
		m_indicatorPlot["pos"] = Utils::doubletostring(getpos(Bar.symbol));
		//m_indicatorPlot["cdp"] = Utils::doubletostring(cdp);
		//m_indicatorPlot["rsi"] = Utils::doubletostring(rsi);
		//m_indicatorPlot["100"] = Utils::doubletostring(count);


		m_VarPlotmtx.unlock();
	}


	//缓存5分钟线
	m_algorithm->checkStop(&Bar);
	m_hour = Bar.gethour();
	m_minute = Bar.getminute();
	
	if (lastHour >= 3)
	{
		if (m_5Bar.close != 0)
		{
			if (!((m_hour == 11 && m_minute == 30) || (m_hour == 15 && m_minute == 00) || (m_hour == 10 && m_minute == 15)))
			{
				on10Bar(m_5Bar);
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
		lastHour = 0;
	}
	else
	{
		m_5Bar.high = std::max(m_5Bar.high, Bar.high);
		m_5Bar.low = std::min(m_5Bar.low, Bar.low);
		m_5Bar.close = Bar.close;
		m_5Bar.volume += Bar.volume;
		m_5Bar.highPrice = Bar.highPrice;
		m_5Bar.lowPrice = Bar.lowPrice;
		lastHour += 1;
	}

	std::unique_lock<std::mutex>lck(m_HLCmtx);
	if (high_vector.size()>225)
	{
		high_vector.erase(high_vector.begin());
		close_vector.erase(close_vector.begin());
		low_vector.erase(low_vector.begin());
	}
	if (!((Bar.gethour() == 9 && Bar.getminute() == 0) || (Bar.gethour() == 9 && Bar.getminute() == 5) ||
		(Bar.gethour() == 21 && Bar.getminute() == 0) || (Bar.gethour() == 21 && Bar.getminute() == 5)))
	{
		close_vector.push_back(Bar.close);
		high_vector.push_back(Bar.high);
		low_vector.push_back(Bar.low);
	}


	if (close_vector.size() < 225)
	{
		return;
	}
	
	if ((m_hour == 9 && m_minute == 01) )
	{
		std::vector<double>::iterator biggest375 = std::max_element(std::begin(close_vector), std::end(close_vector));
		std::vector<double>::iterator smallest375 = std::min_element(std::begin(close_vector), std::end(close_vector));

		
		todayStart = Bar.open;
		upperBorder = todayStart + atoi(getparam("filter").c_str());
		lowerBorder = todayStart - atoi(getparam("filter").c_str());
		delta = upperBorder - lowerBorder;
		if (delta < 20)
		{
			trade = false;
		}
		else
		{
			trade = true;
		}
		wg1 = Bar.open + 1 * (delta / 5);
		wg2 = Bar.open - 1 * (delta / 5);


	}




	/*
	if (m_hour == 9 && m_minute == 01)
	{
		std::vector<double>::iterator biggest375 = std::max_element(std::begin(close_vector), std::end(close_vector));
		std::vector<double>::iterator smallest375 = std::min_element(std::begin(close_vector), std::end(close_vector));
		upperBorder = *biggest375;
		lowerBorder = *smallest375;
		cdp = (upperBorder + lowerBorder + 2 * close_vector[close_vector.size() - 1]) / 4;
		ah = cdp + upperBorder - lowerBorder;
		nh = 2 * cdp - lowerBorder;
		nl = 2 * cdp - upperBorder;
		al = cdp - upperBorder + lowerBorder;
		todayEntry = (upperBorder - lowerBorder > 20);
	}
	*/
	
	if (m_hour == 14 && m_minute >= 55)
	{
		m_algorithm->set_supposedPos(Bar.symbol, 0);
	}
	
	//回测用发单函数
	if (TradingMode == BacktestMode)
	{
		m_algorithm->setTradingMode(BacktestMode);
		m_algorithm->checkPositions_Bar(&Bar);
	}


	putEvent();
}
void qushi_wangge::on10Bar(BarData Bar)
{

	if (trade == true)
	{
		if (Bar.close > lowerBorder && Bar.close <upperBorder)
		{
			if (close_vector[close_vector.size() - 2] > lowerBorder&&close_vector[close_vector.size() - 2] < upperBorder)
			{
				if (Bar.close > wg1&&getpos(Bar.symbol) > -8 && getpos(Bar.symbol) < 0)
				{
					m_algorithm->set_supposedPos(Bar.symbol, getpos(Bar.symbol) * 2);
				}
				else if (Bar.close < wg2&&getpos(Bar.symbol) < 8 && getpos(Bar.symbol) > 0)
				{
					m_algorithm->set_supposedPos(Bar.symbol, getpos(Bar.symbol) * 2);
				}
				else if (Bar.close > wg1&&getpos(Bar.symbol) > 0)
				{
					m_algorithm->set_supposedPos(Bar.symbol, 0);
				}
				else if (Bar.close < wg2&&getpos(Bar.symbol) < 0)
				{
					m_algorithm->set_supposedPos(Bar.symbol, 0);
				}
				else if (Bar.close > wg1&&getpos(Bar.symbol) == 0)
				{
					m_algorithm->set_supposedPos(Bar.symbol, -1);
				}
				else if (Bar.close < wg2&&getpos(Bar.symbol) == 0)
				{
					m_algorithm->set_supposedPos(Bar.symbol, 1);
				}
			}
			else
			{
				if (close_vector[close_vector.size() - 2] < lowerBorder)
				{
					m_algorithm->set_supposedPos(Bar.symbol, 8);
				}
				else if (close_vector[close_vector.size() - 2] >upperBorder)
				{
					m_algorithm->set_supposedPos(Bar.symbol, -8);
				}
			}
		}
		else
		{
			if (Bar.close > todayStart + atoi(getparam("filter").c_str()))
			{
				m_algorithm->set_supposedPos(Bar.symbol, 8);
			}
			else if (Bar.close < todayStart - atoi(getparam("filter").c_str()))
			{
				m_algorithm->set_supposedPos(Bar.symbol, -8);
			}
		}
	}
	/*
	if (getpos(Bar.symbol) == 0 && todayEntry == true && rsi < 80 && cdp > 0 && m_hour < 15)
	{
		if (Bar.close > ah)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 1);
		}
	}
	else if (getpos(Bar.symbol) == 0 && todayEntry == true && rsi > 20 && cdp > 0 && m_hour < 15)
	{
		if (Bar.close < al)
		{
			m_algorithm->set_supposedPos(Bar.symbol, -1);
		}
	}

	if (getpos(Bar.symbol)>0&&(Bar.close > al||Bar.close>nh))
	{
		m_algorithm->set_supposedPos(Bar.symbol, -0);
	}
	if (getpos(Bar.symbol) < 0 && (Bar.close < nl || Bar.close < ah))
	{
		m_algorithm->set_supposedPos(Bar.symbol, -0);
	}

	if (getpos(Bar.symbol) > 0)
	{
		if (Bar.close - openPrice > 90)
		{
			m_algorithm->set_supposedPos(Bar.symbol, -0);
		}
		if (Bar.close - openPrice < -5.6*(upperBorder - lowerBorder))
		{
			m_algorithm->set_supposedPos(Bar.symbol, -0);
		}
	}
	else if (getpos(Bar.symbol) < 0)
	{
		if (openPrice - Bar.close > 90)
		{
			m_algorithm->set_supposedPos(Bar.symbol, -0);
		}
		if (openPrice-Bar.close < -5.6*(upperBorder - lowerBorder))
		{
			m_algorithm->set_supposedPos(Bar.symbol, -0);
		}
	}
	*/
	
}

//报单回调
void qushi_wangge::onOrder(std::shared_ptr<Event_Order>e)
{


}
//成交回调
void qushi_wangge::onTrade(std::shared_ptr<Event_Trade>e)
{
	openPrice = e->price;
	if (getpos(e->symbol) > 0)
	{
		wg1 = openPrice + delta / 5 ;
		wg2 = openPrice - delta / 5 + getpos(e->symbol);
	}
	else if (getpos(e->symbol) < 0)
	{
		wg1 = openPrice + delta / 5 + getpos(e->symbol);
		wg2 = openPrice - delta / 5 ;
	}

}

//更新参数到界面
void qushi_wangge::putEvent()
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
	m_strategydata->insertvar("upperBorder", Utils::doubletostring(upperBorder));
	m_strategydata->insertvar("lowerBorder", Utils::doubletostring(lowerBorder));

	m_strategydata->insertvar("wg1", Utils::doubletostring(wg1));
	m_strategydata->insertvar("wg2", Utils::doubletostring(wg2));

	//将参数和变量传递到界面上去
	std::shared_ptr<Event_UpdateStrategy>e = std::make_shared<Event_UpdateStrategy>();
	e->parammap = m_strategydata->getallparam();
	e->varmap = m_strategydata->getallvar();
	e->strategyname = m_strategydata->getparam("name");
	m_ctamanager->PutEvent(e);
}