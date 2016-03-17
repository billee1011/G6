#include "G6.h"

int			g_exit_flag = 0 ;
int			g_SIGUSR1_flag = 0 ;
int			g_SIGUSR2_flag = 0 ;
int			g_SIGTERM_flag = 0 ;

static void sig_set_flag( int sig_no )
{
	InfoLog( __FILE__ , __LINE__ , "recv signal[%d]" , sig_no );
	
	if( sig_no == SIGUSR1 )
	{
		g_SIGUSR1_flag = 1 ;
	}
	else if( sig_no == SIGUSR2 )
	{
		g_SIGUSR2_flag = 1 ;
	}
	else if( sig_no == SIGTERM )
	{
		g_SIGTERM_flag = 1 ;
	}
	
	return;
}

static void sig_proc( struct ServerEnv *penv )
{
	int		nret = 0 ;
	
	if( g_SIGUSR1_flag == 1 )
	{
		/* ֪ͨ�����̹߳ر���־�ļ������֣�����д��־ʱ���ٴ��Զ��� */
		InfoLog( __FILE__ , __LINE__ , "write accept_request_pipe L ..." );
		nret = write( g_penv->accept_request_pipe.fds[1] , "L" , 1 ) ;
		InfoLog( __FILE__ , __LINE__ , "write accept_request_pipe L done[%d]" , nret );
		
		g_SIGUSR1_flag = 0 ;
	}
	else if( g_SIGUSR2_flag == 1 )
	{
		struct ForwardSession	*forward_session_array = NULL ;
		
		pid_t			pid ;
		char			command ;
		
		int			nret = 0 ;
		
		/* ����ʣ���ڴ� */
		forward_session_array = (struct ForwardSession *)malloc( sizeof(struct ForwardSession) * penv->cmd_para.forward_session_size ) ;
		if( forward_session_array == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "malloc failed , momery not enough , errno[%d]" , errno );
			return;
		}
		free( forward_session_array );
		
		/* ��������socket������������ */
		nret = SaveListenSockets( g_penv ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "SaveListenSockets faild[%d]" , nret );
			return;
		}
		
		/* ֪ͨ�����̹߳ر�����socket */
		InfoLog( __FILE__ , __LINE__ , "write accept_request_pipe Q ..." );
		nret = write( g_penv->accept_request_pipe.fds[1] , "Q" , 1 ) ;
		InfoLog( __FILE__ , __LINE__ , "write accept_request_pipe Q done[%d]" , nret );
		
		InfoLog( __FILE__ , __LINE__ , "read accept_response_pipe ..." );
		nret = read( g_penv->accept_response_pipe.fds[0] , & command , 1 ) ;
		InfoLog( __FILE__ , __LINE__ , "read accept_response_pipe done[%d][%c]" , nret , command );
		
		/* �����ӽ��̣����³���ӳ�񸲸Ǵ���� */
		pid = fork() ;
		if( pid == -1 )
		{
			;
		}
		else if( pid == 0 )
		{
			execvp( "G6" , g_penv->argv );
			
			exit(9);
		}
		
		/* �����˳���־ */
		g_SIGUSR2_flag = 0 ;
		g_exit_flag = 1 ;
		DebugLog( __FILE__ , __LINE__ , "set g_exit_flag[%d]" , g_exit_flag );
	}
	else if( g_SIGTERM_flag == 1 )
	{
		char		command ;
		
		int		nret = 0 ;
		
		/* ֪ͨ�����̹߳ر�����socket */
		InfoLog( __FILE__ , __LINE__ , "write accept_request_pipe Q ..." );
		nret = write( g_penv->accept_request_pipe.fds[1] , "Q" , 1 ) ;
		InfoLog( __FILE__ , __LINE__ , "write accept_request_pipe Q done[%d]" , nret );
		
		InfoLog( __FILE__ , __LINE__ , "read accept_response_pipe ..." );
		nret = read( g_penv->accept_response_pipe.fds[0] , & command , 1 ) ;
		InfoLog( __FILE__ , __LINE__ , "read accept_response_pipe done[%d][%c]" , nret , command );
		
		/* �����˳���־ */
		g_SIGTERM_flag = 0 ;
		g_exit_flag = 1 ;
		DebugLog( __FILE__ , __LINE__ , "set g_exit_flag[%d]" , g_exit_flag );
	}
	
	return;
}

int MonitorProcess( struct ServerEnv *penv )
{
	struct sigaction	act ;
	
	pid_t			pid ;
	int			status ;
	
	int			nret = 0 ;
	
	/* ������־����ļ� */
	INIT_TIME
	SETPID
	SETTID
	InfoLog( __FILE__ , __LINE__ , "--- G6.MonitorProcess ---" );
	
	/* �����źž�� */
	act.sa_handler = & sig_set_flag ;
	sigemptyset( & (act.sa_mask) );
	act.sa_flags = 0 ;
	
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	signal( SIGPIPE , SIG_IGN );
	sigaction( SIGTERM , & act , NULL );
	sigaction( SIGUSR1 , & act , NULL );
	sigaction( SIGUSR2 , & act , NULL );
	
	/* ������ѭ�� */
	while( g_exit_flag == 0 )
	{
		/* �����ӽ��� */
		_FORK :
		penv->pid = fork() ;
		if( penv->pid == -1 )
		{
			if( errno == EINTR )
			{
				sig_proc( penv );
				goto _FORK;
			}
			
			ErrorLog( __FILE__ , __LINE__ , "fork failed , errno[%d]" , errno );
			return -1;
		}
		else if( penv->pid == 0 )
		{
			INIT_TIME
			SETPID
			SETTID
			
			InfoLog( __FILE__ , __LINE__ , "child : [%ld]fork[%ld]" , getppid() , getpid() );
			
			close( penv->accept_request_pipe.fds[1] );
			close( penv->accept_response_pipe.fds[0] );
			
			nret = WorkerProcess( penv ) ;
			
			CleanEnvirment( penv );
			
			exit(-nret);
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "parent : [%ld]fork[%ld]" , getpid() , penv->pid );
			
			close( penv->accept_request_pipe.fds[0] );
			close( penv->accept_response_pipe.fds[1] );
		}
		
		if( penv->cmd_para.close_log_flag == 1 )
			CloseLogFile();
		
		/* ����ӽ��̽��� */
		_WAITPID :
		pid = waitpid( -1 , & status , 0 );
		INIT_TIME
		if( pid == -1 )
		{
			if( errno == EINTR )
			{
				sig_proc( penv );
				goto _WAITPID;
			}
			
			ErrorLog( __FILE__ , __LINE__ , "waitpid failed , errno[%d]" , errno );
			close( penv->accept_request_pipe.fds[1] );
			close( penv->accept_response_pipe.fds[0] );
			return -1;
		}
		if( pid != penv->pid )
			goto _WAITPID;
		
		if( WCOREDUMP(status) )
		{
			ErrorLog( __FILE__ , __LINE__
				, "waitpid[%ld] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d] WCOREDUMP[%d]"
				, penv->pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) , WCOREDUMP(status) );
		}
		else
		{
			InfoLog( __FILE__ , __LINE__
				, "waitpid[%ld] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d] WCOREDUMP[%d]"
				, penv->pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) , WCOREDUMP(status) );
		}
		
		if( g_exit_flag == 0 )
			sleep(1);
	}
	
	return 0;
}

int _MonitorProcess( void *pv )
{
	return MonitorProcess( (struct ServerEnv *)pv );
}
