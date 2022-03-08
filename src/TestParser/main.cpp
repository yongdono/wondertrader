#include <iostream>

#include "../Includes/IParserApi.h"
#include "../Includes/WTSVariant.hpp"
#include "../Includes/WTSContractInfo.hpp"
#include "../Includes/WTSDataDef.hpp"

#include "../Share/TimeUtils.hpp"
#include "../Share/StdUtils.hpp"

#include "../WTSTools/WTSBaseDataMgr.h"
#include "../WTSTools/WTSLogger.h"
#include "../WTSUtils/WTSCfgLoader.h"

WTSBaseDataMgr	g_bdMgr;

USING_NS_WTP;

template<typename... Args>
void log(const char* format, const Args& ...args)
{
	WTSLogger::log_raw(LL_INFO, fmt::format(format, args...).c_str());
}

class ParserSpi : public IParserSpi
{
public:
	ParserSpi(){}

	bool init(WTSVariant* params, const char* ttype)
	{
		m_pParams = params;
		if (m_pParams)
			m_pParams->retain();

		m_strModule = ttype;
		return true;
	}

	void release()
	{
		if (_api)
		{
			_api->release();
		}
	}

	void run(bool bRestart = false)
	{
		if (!createParser(m_strModule.c_str()))
		{
			return;
		}

		_api->registerSpi(this);

		if (!_api->init(m_pParams))
		{
			return;
		}

		ContractSet contractSet;
		WTSArray* ayContract = g_bdMgr.getContracts();
		WTSArray::Iterator it = ayContract->begin();
		for (; it != ayContract->end(); it++)
		{
			WTSContractInfo* contract = STATIC_CONVERT(*it, WTSContractInfo*);
			WTSCommodityInfo* pCommInfo = g_bdMgr.getCommodity(contract);
			contractSet.insert(contract->getFullCode());
		}

		ayContract->release();
		_api->subscribe(contractSet);
		_api->connect();
	}

	bool createParser(const char* moduleName)
	{
		HINSTANCE hInst = LoadLibrary(moduleName);
		if (hInst == NULL)
		{
			log("ģ��{}����ʧ��", moduleName);
			return false;
		}

		FuncCreateParser pCreator = (FuncCreateParser)GetProcAddress(hInst, "createParser");
		if (NULL == pCreator)
		{
			log("�ӿڴ���������ȡʧ��");
			return false;
		}

		_api = pCreator();
		if (NULL == _api)
		{
			log("�ӿڴ���ʧ��");
			return false;
		}

		m_funcRemover = (FuncDeleteParser)GetProcAddress(hInst, "deleteParser");
		return true;
	}

public:
	virtual void handleParserLog(WTSLogLevel ll, const char* message) override
	{
		WTSLogger::log_raw(ll, message);
	}

	virtual void handleQuote(WTSTickData *quote, uint32_t procFlag) override
	{
		log("{}@{}.{}, price:{}, voume:{}", quote->code(), quote->actiondate(), quote->actiontime(), quote->price(), quote->totalvolume());
	}

	virtual void handleSymbolList(const WTSArray* aySymbols) override
	{

	}

public:
	virtual IBaseDataMgr*	getBaseDataMgr()
	{
		return &g_bdMgr;
	}
	

private:
	IParserApi*			_api;
	FuncDeleteParser	m_funcRemover;
	std::string			m_strModule;
	WTSVariant*			m_pParams;
};

std::string getBaseFolder()
{
	static std::string basePath;
	if (basePath.empty())
	{
		char path[MAX_PATH] = { 0 };
		GetModuleFileName(GetModuleHandle(NULL), path, MAX_PATH);

		basePath = path;
		auto pos = basePath.find_last_of('\\');
		basePath = basePath.substr(0, pos + 1);
	}

	return basePath;
}

int main()
{
	WTSLogger::init();

	WTSLogger::info("�����ɹ�,��ǰϵͳ�汾��: v1.0");

	WTSVariant* root = WTSCfgLoader::load_from_file("config.yaml", true);
	if (root == NULL)
	{
		WTSLogger::log_raw(LL_ERROR, "�����ļ�config.yaml����ʧ��");
		return 0;
	}

	WTSVariant* cfg = root->get("config");
	bool isUTF8 = cfg->getBoolean("utf8");
	if (cfg->has("session"))
		g_bdMgr.loadSessions(cfg->getCString("session"), isUTF8);

	if (cfg->has("commodity"))
		g_bdMgr.loadCommodities(cfg->getCString("commodity"), isUTF8);

	if (cfg->has("contract"))
		g_bdMgr.loadContracts(cfg->getCString("contract"), isUTF8);

	std::string module = cfg->getCString("parser");
	std::string profile = cfg->getCString("profile");
	WTSVariant* params = root->get(profile.c_str());
	if (params == NULL)
	{
		WTSLogger::error_f("������{}������", profile);
		return 0;
	}

	ParserSpi* parser = new ParserSpi;
	parser->init(params, module.c_str());
	parser->run();

	root->release();

	getchar();
	
	//exit(9);
	parser->release();
	delete parser;

	return 0;
}