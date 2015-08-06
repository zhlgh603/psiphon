/*
 * Copyright (c) 2015, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package com.psiphon3.psiphonlibrary;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.ListIterator;

import org.apache.http.auth.Credentials;
import org.apache.http.auth.NTCredentials;
import org.json.JSONObject;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import com.psiphon3.psiphonlibrary.Utils.MyLog;

public class PsiphonData
{
    // Singleton pattern
    
    private static PsiphonData m_psiphonData;

    public Object clone() throws CloneNotSupportedException
    {
        throw new CloneNotSupportedException();
    }
    
    public static synchronized PsiphonData getPsiphonData()
    {
        if (m_psiphonData == null)
        {
            m_psiphonData = new PsiphonData();
        }
        
        return m_psiphonData;
    }

    private boolean m_useHTTPProxy;
    private boolean m_useSystemProxySettings;
    private boolean m_useCustomProxySettings;
    private String m_customProxyHost;
    private String m_customProxyPort;
    private boolean m_useProxyAuthentication;
    private String m_proxyUsername;
    private String m_proxyPassword;
    private String m_proxyDomain;
    private ProxySettings m_savedSystemProxySettings;

    private PsiphonData()
    {
        m_useHTTPProxy = false;
        m_useSystemProxySettings = false;
        m_useCustomProxySettings = false;
        m_useProxyAuthentication = false;
    }

    public synchronized void setUseHTTPProxy(boolean useHTTPProxy)
    {
    	m_useHTTPProxy = useHTTPProxy;
    }

    public synchronized boolean getUseHTTPProxy()
    {
    	return m_useHTTPProxy;
    }
    
    public synchronized void setUseSystemProxySettings(boolean useSystemProxySettings)
    {
        m_useSystemProxySettings = useSystemProxySettings;
    }

    public synchronized boolean getUseSystemProxySettings()
    {
        return m_useSystemProxySettings;
    }
    
    public synchronized void setUseCustomProxySettings(boolean useCustomProxySettings)
    {
        m_useCustomProxySettings = useCustomProxySettings;
    }

    public synchronized boolean getUseCustomProxySettings()
    {
        return m_useCustomProxySettings;
    }
    
    public synchronized void setCustomProxyHost(String host)
    {
    	m_customProxyHost = host;
    }
    
    public synchronized String getCustomProxyHost()
    {
    	return m_customProxyHost;
    }

    public synchronized void setCustomProxyPort(String port)
    {
    	m_customProxyPort = port;
    }
    
    public synchronized String getCustomProxyPort()
    {
    	return m_customProxyPort;
    }
    
    public synchronized void setUseProxyAuthentication(boolean useProxyAuthentication)
    {
        m_useProxyAuthentication = useProxyAuthentication;
    }

    public synchronized boolean getUseProxyAuthentication()
    {
        return m_useProxyAuthentication;
    }

    public synchronized void setProxyUsername(String username)
    {
    	m_proxyUsername = username;
    }
    
    public synchronized String getProxyUsername()
    {
    	return m_proxyUsername;
    }

    public synchronized void setProxyPassword(String password)
    {
    	m_proxyPassword = password;
    }
    
    public synchronized String getProxyPassword()
    {
    	return m_proxyPassword;
    }
    
    public synchronized void setProxyDomain(String domain)
    {
    	m_proxyDomain = domain;
    }
    
    public synchronized String getProxyDomain()
    {
    	return m_proxyDomain;
    }

    
    public class ProxySettings
    {
        public String proxyHost;
        public int proxyPort;
    }
    
    // Call this before doing anything that could change the system proxy settings
    // (such as setting a WebView's proxy)
    public synchronized void saveSystemProxySettings(Context context)
    {
        if (m_savedSystemProxySettings == null)
        {
            m_savedSystemProxySettings = getSystemProxySettings(context);
        }
    }
    
    // Checks if we are supposed to use proxy settings, custom or system,
    // and if system, if any system proxy settings are configured.
    // Returns the user-requested proxy settings.
    public synchronized ProxySettings getProxySettings(Context context)
    {
        if (!getUseHTTPProxy())
        {
            return null;
        }
        
        ProxySettings settings = null;
        
        if (getUseCustomProxySettings())
        {
            settings = new ProxySettings();
            
            settings.proxyHost = getCustomProxyHost();
            String port = getCustomProxyPort();
            try
            {
                settings.proxyPort = Integer.parseInt(port);
            }
            catch (NumberFormatException e)
            {
                settings.proxyPort = -1;
            }
        }
        		
        if (getUseSystemProxySettings())
        {
            settings = getSystemProxySettings(context);
            
            if (settings.proxyHost == null || 
                settings.proxyHost.length() == 0 || 
                settings.proxyPort <= 0)
            {
                settings = null;
            }
        }
        
        return settings;
    }
    
	public synchronized Credentials getProxyCredentials() {
		if (!getUseProxyAuthentication()) {
			return null;
		}

		String username = getProxyUsername();
		String password = getProxyPassword();
		String domain = getProxyDomain();
		
		if (username == null || username.trim().equals("")) {
			return null;
		}
		if (password == null || password.trim().equals("")) {
			return null;
		}
		if (domain == null || domain.trim().equals("")) {
			return new NTCredentials(username, password, "", "");
		}
		
		String localHost;
		try {
			localHost = InetAddress.getLocalHost().getHostName();
		} catch (UnknownHostException e) {
			localHost = "localhost";
		}

		return new NTCredentials(username, password, localHost, domain);
	}
    
    private ProxySettings getSystemProxySettings(Context context)
    {
        ProxySettings settings = m_savedSystemProxySettings;
        
        if (settings == null)
        {
            settings = new ProxySettings();
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
            {
                settings.proxyHost = System.getProperty("http.proxyHost");
                String port = System.getProperty("http.proxyPort");
                settings.proxyPort = Integer.parseInt(port != null ? port : "-1");
            }
            else
            {
                settings.proxyHost = android.net.Proxy.getHost(context);
                settings.proxyPort = android.net.Proxy.getPort(context);
            }
        }
        
        return settings;
    }
    
    /*
     * Status Message History support
     */

    static public class StatusEntry
    {
        private Date timestamp;
        private int id;
        private Object[] formatArgs;
        private Throwable throwable;
        private int priority;
        private MyLog.Sensitivity sensitivity;
        
        public Date timestamp()
        {
            return timestamp;
        }
        
        public int id()
        {
            return id;
        }
        
        public Object[] formatArgs()
        {
            return formatArgs;
        }
        
        public Throwable throwable()
        {
            return throwable;
        }
        
        public int priority()
        {
            return priority;
        }
        
        public MyLog.Sensitivity sensitivity()
        {
            return sensitivity;
        }
    }
    
    private ArrayList<StatusEntry> m_statusHistory = new ArrayList<StatusEntry>();
    
    public void addStatusEntry(
            Date timestamp,
            int id, 
            MyLog.Sensitivity sensitivity, 
            Object[] formatArgs, 
            Throwable throwable, 
            int priority)
    {
        StatusEntry entry = new StatusEntry();
        entry.timestamp = timestamp;
        entry.id = id;
        entry.sensitivity = sensitivity;
        entry.formatArgs = formatArgs;
        entry.throwable = throwable;
        entry.priority = priority;
        
        synchronized(m_statusHistory) 
        {
            m_statusHistory.add(entry);
        }
    }
    
    public ArrayList<StatusEntry> cloneStatusHistory()
    {
        ArrayList<StatusEntry> copy;
        synchronized(m_statusHistory) 
        {
            copy = new ArrayList<StatusEntry>(m_statusHistory);
        }
        return copy;
    }
    
    public void clearStatusHistory()
    {
        synchronized(m_statusHistory) 
        {        
            m_statusHistory.clear();
        }
    }
    
    /** 
     * @param index
     * @return Returns item at `index`. Negative indexes count from the end of 
     * the array. If `index` is out of bounds, null is returned.
     */
    public StatusEntry getStatusEntry(int index) 
    {
        synchronized(m_statusHistory) 
        {   
            if (index < 0) 
            {
                // index is negative, so this is subtracting...
                index = m_statusHistory.size() + index;
                // Note that index is still negative if the array is empty or if
                // the negative value was too large.
            }
            
            if (index >= m_statusHistory.size() || index < 0)
            {
                return null;
            }
            
            return m_statusHistory.get(index);
        }
    }
    
    /** 
     * @return Returns the last non-DEBUG, non-WARN(ing) item, or null if there is none.
     */
    public StatusEntry getLastStatusEntryForDisplay() 
    {
        synchronized(m_statusHistory) 
        {   
            ListIterator<StatusEntry> iterator = m_statusHistory.listIterator(m_statusHistory.size());
            
            while (iterator.hasPrevious())
            {
                StatusEntry current_item = iterator.previous();
                if (current_item.priority() != Log.DEBUG &&
                    current_item.priority() != Log.WARN)
                {
                    return current_item;
                }
            }
            
            return null;
        }
    }
    
    /*
     * Diagnostic history support
     */
    
    static public class DiagnosticEntry extends Object
    {
        private Date timestamp;
        private String msg;
        private JSONObject data;

        public Date timestamp()
        {
            return timestamp;
        }
        
        public String msg()
        {
            return msg;
        }
        
        public JSONObject data()
        {
            return data;
        }
    }
        
    static private List<DiagnosticEntry> m_diagnosticHistory = new ArrayList<DiagnosticEntry>();

    static public void addDiagnosticEntry(Date timestamp, String msg, JSONObject data)
    {
        DiagnosticEntry entry = new DiagnosticEntry();
        entry.timestamp = timestamp;
        entry.msg = msg;
        entry.data = data;
        synchronized(m_diagnosticHistory) 
        {
            m_diagnosticHistory.add(entry);
        }
    }
    
    static public List<DiagnosticEntry> cloneDiagnosticHistory()
    {
        List<DiagnosticEntry> copy;
        synchronized(m_diagnosticHistory) 
        {
            copy = new ArrayList<DiagnosticEntry>(m_diagnosticHistory);
        }
        return copy;
    }
}
