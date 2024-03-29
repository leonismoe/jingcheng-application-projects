#include "stdafx.h"
#include "plugin_manager.h"
#include "application.h"

#include "plugin_default.h"

LOCAL_LOGGER_ENABLE(_T("PluginMan"), LOGGER_LEVEL_DEBUGINFO);

///////////////////////////////////////////////////////////////////////////////
//-- CVariableContainer
CVariableContainer::CVariableContainer(void)
	: m_global_var(NULL)
{
	jcparam::CParamSet::Create(m_global_var);
	JCASSERT(m_global_var);
}

CVariableContainer::~CVariableContainer(void)
{
	if (m_global_var) m_global_var->Release();
}

void CVariableContainer::GetSubValue(LPCTSTR name, IValue * & val)
{
	CSvtApplication * app = CSvtApplication::GetApp();
	JCASSERT(app);
	bool br = app->GetDefaultVariable(name, val);

	if (!br)
	{
		JCASSERT(m_global_var);
		m_global_var->GetSubValue(name, val);
	}
}

void CVariableContainer::SetSubValue(LPCTSTR name, IValue * val)
{
	m_global_var->SetSubValue(name, val);
}

///////////////////////////////////////////////////////////////////////////////
//-- CPluginManager
CPluginManager::CPluginManager(void)
	: m_default_plugin (NULL)
	, m_vars(NULL)
{
	m_vars = new CVariableContainer;

	m_default_plugin = static_cast<jcscript::IPlugin*>(
		new CPluginDefault);

}

CPluginManager::~CPluginManager(void)
{
	PLUGIN_ITERATOR it = m_plugin_map.begin();
	PLUGIN_ITERATOR endit = m_plugin_map.end();
	for ( ; it != endit; ++ it)
	{
		CPluginInfo * info = it->second;
		delete info;
	}
	if (m_default_plugin) m_default_plugin->Release();

	if (m_vars)	m_vars->Release();
}

bool CPluginManager::RegistPlugin(const CJCStringT & name, CDynamicModule * module, DWORD _property, PLUGIN_CREATOR creator)
{
	LOG_STACK_TRACE();
	CPluginInfo * info = new CPluginInfo(name, module, _property, creator);
	std::pair<PLUGIN_ITERATOR, bool> rs = m_plugin_map.insert(PLUGIN_INFO_PAIR(name, info));
	if (!rs.second)
	{
		delete info;
		THROW_ERROR(ERR_APP, _T("Plugin %s has already registed"), name.c_str() );
	}
	LOG_DEBUG(_T("Plugin %s ptr=0x%08x registed"), name.c_str(), (DWORD)(creator));
	return true;
}

bool CPluginManager::RegistPlugin(jcscript::IPlugin * plugin)
{
	LOG_STACK_TRACE();
	JCASSERT(plugin);

	LPCTSTR name = plugin->name();
	LOG_TRACE(_T("plugin: %s, ptr = 0x%08X"), name, (UINT)(plugin));

	CPluginInfo * info = new CPluginInfo(name, NULL, CPluginInfo::PIP_SINGLETONE, NULL);
	info->m_object = plugin;
	plugin->AddRef();

	std::pair<PLUGIN_ITERATOR, bool> rs = m_plugin_map.insert(PLUGIN_INFO_PAIR(name, info));
	if (!rs.second)
	{
		delete info;
		THROW_ERROR(ERR_APP, _T("Plugin %s has already registed"), name );
	}
	return true;
}

bool CPluginManager::GetPlugin(const CJCStringT & name, jcscript::IPlugin * & plugin)
{
	JCASSERT(plugin == NULL);

	if ( name.empty() )
	{
		plugin = m_default_plugin;
		plugin->AddRef();
	}
	else
	{
		PLUGIN_ITERATOR it = m_plugin_map.find(name);
		if ( it == m_plugin_map.end() ) return false;
		CPluginInfo * info = it->second;
		if (info->m_property & CPluginInfo::PIP_SINGLETONE)
		{
			if (NULL == info->m_object)
			{
				(info->m_creator)(NULL, info->m_object);
			}
			plugin = info->m_object;
			plugin->AddRef();
		}
		else
		{
			(info->m_creator)(NULL, plugin);
		}
	}
	return true;
}

bool CPluginManager::ReleasePlugin(jcscript::IPlugin* plugin)
{
	return false;
}

bool CPluginManager::RegistDefaultPlugin(jcscript::IPlugin * plugin)
{
	JCASSERT(NULL == m_default_plugin);
	m_default_plugin = plugin;
	if (m_default_plugin) m_default_plugin->AddRef();
	return true;
}

void CPluginManager::GetVarOp(jcscript::IAtomOperate * & op)
{
	LOG_STACK_TRACE();
	JCASSERT(NULL == op);
	//jcscript::CSyntaxParser::CreateVarOp(m_vars, op);
	jcscript::CreateVarOp(m_vars, op);
}

bool CPluginManager::ReadFileOp(LPCTSTR type, const CJCStringT & filename, jcscript::IAtomOperate *& op)
{
	LOG_STACK_TRACE();
	JCASSERT(NULL == op);

	//if ( _tcscmp(_T("busd"), type) == 0 )
	//{
	//	CBusDoctorOp * _op = new CBusDoctorOp(filename);
	//	op = static_cast<jcscript::IAtomOperate*>(_op);
	//}
	return true;
}


