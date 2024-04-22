// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;
using System.Web.Routing;

namespace CISStatus
{
    public class MvcApplication : System.Web.HttpApplication
    {
		public class CISDomainConstraint : IRouteConstraint
		{
			public bool Match( HttpContextBase HttpContext, Route route, string parameterName, RouteValueDictionary values, RouteDirection routeDirection )
			{
#if DEBUG
				return true;
#else
				return HttpContext.Request.Url.Host.ToLower().StartsWith( "cisstatus" );
#endif
			}
		}

		public class VerificationDomainConstraint : IRouteConstraint
		{
			public bool Match( HttpContextBase HttpContext, Route route, string parameterName, RouteValueDictionary values, RouteDirection routeDirection )
			{
				return HttpContext.Request.Url.Host.ToLower().StartsWith( "verificationstatus" );
			}
		}

		public static void RegisterGlobalFilters( GlobalFilterCollection Filters )
		{
			Filters.Add( new HandleErrorAttribute() );
		}

        public static void RegisterRoutes( RouteCollection Routes )
        {
			Routes.IgnoreRoute( "{resource}.axd/{*pathInfo}" );

			Routes.MapRoute(
				  "CISStatus",
				  "{controller}/{action}/{id}",
				  new
				  {
					  controller = "CISStatus",
					  action = "Index",
					  id = ""
				  },
				  new
				  {
				  	  controller = new CISDomainConstraint()
				  }
			);

			Routes.MapRoute(
				  "Verification",
				  "{controller}/{action}/{id}",
				  new
				  {
					  controller = "VerificationStatus",
					  action = "Index",
					  id = ""
				  },
				  new
				  {
					  controller = new VerificationDomainConstraint()
				  }
			);

			Routes.MapRoute(
				  "Default",
				  "{controller}/{action}/{id}",
				  new
				  {
					  controller = "BuildStatus",
					  action = "Index",
					  id = ""
				  }
			);
        }

        protected void Application_Start()
        {
            AreaRegistration.RegisterAllAreas();

			RegisterGlobalFilters( GlobalFilters.Filters );
			RegisterRoutes( RouteTable.Routes );
        }
    }
}