#ifndef _SW_LOG_H_
#define _SW_LOG_H_ 1

#define LOG_NONE      0  //不记录日志
#define LOG_FAULT      1  //致命错误，会导致返回错误或引起程序异常
#define LOG_ERROR     2  //一般错误，内部错误不向上层返回错误，建议解决
#define LOG_WARN   3  //警告信息，不是期望的结果，不会引起错误，但要用户引起重视
#define LOG_INFO      4  //重要变量，有助于解决问题
#define LOG_DEBUG     5  //调试信息，可打印二进制数据
#define LOG_TRACE     6  //跟踪执行，用于跟踪逻辑判断

#define DEFAULT_LOG_MODULE "sget"

extern unsigned int log_level;     //Global

#define LOG(lvl, rv, msg) \
        do { \
        if ((lvl) <= log_level) {\
        	LogMessage(DEFAULT_LOG_MODULE, lvl, __FILE__, __LINE__, rv, msg);} \
        } while (0)

void LogMessage(char* sModule, int nLogLevel, char *sFile,int nLine,unsigned int unErrCode, char *sMessage);

int errorReturn(int errorCode,char *tag,char *msg);

#define LOG_WITH_TAG(lvl, rv, tag,msg) LogMessage(lvl, tag, __FILE__, __LINE__, rv, msg)

#define SIM_TRACE_TAG(tag,msg)	LOG_WITH_TAG(LOG_TRACE,0,tag,msg)

#define SIM_TRACE(msg)	LOG_WITH_TAG(LOG_TRACE,0,DEF_LOG_MODULE,msg)

#define SIM_ERROR_TAG(tag,rv,msg)	LOG_WITH_TAG(LOG_ERROR,rv,tag,msg)

#define SIM_ERROR(rv,msg)	LOG_WITH_TAG(LOG_ERROR,rv,DEF_LOG_MODULE,msg)

#define SIM_WARN_TAG(tag,rv,msg)	LOG_WITH_TAG(LOG_WARN,rv,tag,msg)

#define SIM_WARN(rv,msg)	LOG_WITH_TAG(LOG_WARNING,rv,DEF_LOG_MODULE,msg)

#define SIM_INFO_TAG(tag,rv,msg)	LOG_WITH_TAG(LOG_INFO,rv,tag,msg)

#define SIM_INFO(rv,msg)	LOG_WITH_TAG(LOG_INFO,rv,DEF_LOG_MODULE,msg)

#define ERROR_RETURN(rv,msg) errorReturn(rv,DEF_LOG_MODULE,msg)
#define ERROR_RETURN_TAG(rv,tag,msg) errorReturn(rv,tag,msg)

#endif /*#ifndef _SW_LOG_H_*/

