<%@ LANGUAGE = VBScript %>
<% 'Option Explicit %>
<!-- #include file="../directives.inc" -->

<% const LARGE = "1" %>

<HTML>
<HEAD>
<TITLE><%= L_TITLE_TEXT %></TITLE>
</HEAD>

<% if Session("JSFONTSIZE") = LARGE then %>
	<FRAMESET COLS="100%" ROWS="30,*,50,0" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0>
<% else %>
	<FRAMESET COLS="100%" ROWS="30,*,50,0" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0>
<% end if %>
	<FRAME NAME="head" SRC="JSBrwHd.asp" SCROLLING="no" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0 MARGINHEIGHT=2 MARGINWIDTH = 2>
	<FRAMESET COLS="5,*,5">
		<FRAME NAME="pad" SRC="blank.htm"" SCROLLING="no" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0>
		<FRAME NAME="list" SRC="blank.htm"" SCROLLING="yes" BORDER = 1 FRAMEBORDER = 1 FRAMESPACING = 2 MARGINHEIGHT=0 MARGINWIDTH =0> 
		<FRAME NAME="pad" SRC="blank.htm" SCROLLING="no" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0>
	</FRAMESET>
	<FRAME NAME="tool" SRC="jsbrwflt.asp" SCROLLING="no" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0>	
	<FRAME NAME="hlist" SRC="blank.htm" SCROLLING="no" BORDER = 0 FRAMEBORDER = 0 FRAMESPACING = 0>
</FRAMESET>

</HTML>