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

import android.os.Build;

public class PsiphonConstants
{
    public static Boolean DEBUG = false; // may be changed by activity

    public final static String TAG = "Psiphon";

    public static final String REGION_CODE_ANY = "";

    // The character restrictions are dictated by the server.
    public final static String PLATFORM = ("Android_" + Build.VERSION.RELEASE).replaceAll("[^\\w\\-\\.]", "_");

    public final static String ROOTED = "_rooted";

    public final static String PLAY_STORE_BUILD = "_playstore";

    public final static String GOOGLE_PLAY_LICENSING_PUBLIC_KEY = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA6Ng/B/vTGijsipqy6Cmwtgt7ah3lePQPVCmOZNvwf+aBAMoRXeHVjVQgTu/acbc1IMnuQo62vNgcTUV8LE7v6vB1oxvnOHz3aFGuQ+ZgwsnzGD6G9RzBUW1UdzjX6RpdjHSjg2LREdD6K8JnVCapvCbzHmVgTHL/OOKQQS8vIwB1nr/gemxrE0WzCuYUE5uQHYKyHLvGmQEI5jO/RLKvCT/E/1v3n9wHfNQZhAUV3+0eb2EiU0QCh/sjwK6920Hwvpm/fAqeBTNDCkklIZxxPzoDyZyLw+t/K6z7IJHFDzgPquoMLPs9IM/In51828o2b7/PccUZWmuCuB/Vz8NskwIDAQAB";

    public final static byte[] GOOGLE_PLAY_LICENSING_SALT ={-46, 65, 30, -128, -103, -57, 74, -64, 51, 88, -95, -45, 77, -117, -36, -113, -11, 32, -64, 89};
}
