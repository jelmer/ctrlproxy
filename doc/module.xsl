<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:ctrlproxy="http://ctrlproxy.vernstok.nl/common"
				version="1.1"
                extension-element-prefixes="exsl">

<xsl:output method="xml" omit-xml-declaration="yes"/>

<xsl:param name="xmlCtrlproxyNsUri" select="'http://ctrlproxy.vernstok.nl/common'"/>

<xsl:template match="ctrlproxy:module">
	<xsl:element name="chapter">
		<xsl:attribute name="id">
			<xsl:value-of select="@name"/>
		</xsl:attribute>

		<xsl:if test="title = ''">
			<xsl:element name="title">
				<xsl:value-of select="@name"/><xsl:text> module</xsl:text>
			</xsl:element>
		</xsl:if>
		
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="modulemeta">
	<xsl:if test="description != ''">
		<xsl:element name="para">
			<xsl:value-of select="description"/>
		</xsl:element>
	</xsl:if>

	<xsl:element name="variablelist">
		<xsl:if test="version != ''">
			<!--Version -->
			<xsl:element name="varlistentry">
				<xsl:element name="term">Version:</xsl:element>
				<xsl:element name="listitem"><xsl:element name="para"><xsl:value-of select="version"/></xsl:element></xsl:element>
			</xsl:element>
		</xsl:if>

		<xsl:if test="author != ''">
			<!--Author-->
			<xsl:element name="varlistentry">
				<xsl:element name="term">Author:</xsl:element>
				<xsl:element name="listitem"><xsl:element name="para">
				<xsl:element name="ulink">
					<xsl:attribute name="url"><xsl:text>mailto:</xsl:text><xsl:value-of select="email"/></xsl:attribute>
				<xsl:value-of select="author"/></xsl:element></xsl:element>
				</xsl:element>
			</xsl:element>
		</xsl:if>

		<xsl:if test="homepage != ''">
			<!--Homepage-->
			<xsl:element name="varlistentry">
				<xsl:element name="term">Homepage:</xsl:element>
					<xsl:element name="listitem"><xsl:element name="para">
						<xsl:element name="ulink">
							<xsl:attribute name="url"><xsl:value-of select="homepage"/></xsl:attribute>
							<xsl:value-of select="homepage"/>
						</xsl:element>
					</xsl:element>
				</xsl:element>
			</xsl:element>
		</xsl:if>
		
		<xsl:if test="requirements != ''">
			<!--Requirements-->
			<xsl:element name="varlistentry">
				<xsl:element name="term">Requirements:</xsl:element>
				<xsl:element name="listitem">
					<xsl:element name="para">
						<xsl:value-of select="requirements"/>
					</xsl:element>
				</xsl:element>
			</xsl:element>
		</xsl:if>
	</xsl:element>
</xsl:template>

<xsl:template match="element">
	<xsl:element name="varlistentry">
		<xsl:element name="term"><xsl:element name="emphasis"><xsl:value-of select="@name"/></xsl:element></xsl:element>
		<xsl:element name="listitem">
			<xsl:element name="para">
				<xsl:value-of select="description"/>
			</xsl:element>
			<xsl:if test="element != '' or attribute != ''">
				<xsl:element name="variablelist">
					<xsl:apply-templates/>
				</xsl:element>
			</xsl:if>
		</xsl:element>
	</xsl:element>
</xsl:template>

<xsl:template match="element/description"></xsl:template>

<xsl:template match="attribute">
	<xsl:element name="varlistentry">
		<xsl:element name="term"><xsl:element name="emphasis"><xsl:value-of select="@name"/></xsl:element></xsl:element>
		<xsl:element name="listitem">
			<xsl:element name="para">
				<xsl:value-of select="description"/>
			</xsl:element>
		</xsl:element>
	</xsl:element>
</xsl:template>

<xsl:template match="configuration">
	<xsl:element name="sect1">
		<xsl:element name="title"><xsl:text>Configuration</xsl:text></xsl:element>

		<xsl:element name="para"><xsl:text>The following XML elements are supported:</xsl:text></xsl:element>

		<xsl:if test="element != '' or attribute != ''">
			<xsl:element name="variablelist">
				<xsl:apply-templates/>
			</xsl:element>
		</xsl:if>
	</xsl:element>
</xsl:template>

<xsl:template match="ctrlproxy:module/description">
	<xsl:element name="sect1">
		<xsl:element name="title"><xsl:text>Description</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="ctrlproxy:module/example">
	<xsl:element name="sect1">
		<xsl:element name="title"><xsl:text>Example configuration</xsl:text></xsl:element>
		<xsl:element name="programlisting">
			<xsl:apply-templates/>
		</xsl:element>
	</xsl:element>
</xsl:template>

<xsl:template match="section">
	<xsl:element name="sect1">
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="subsection">
	<xsl:element name="sect2">
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="todo">
	<xsl:element name="sect1">
		<xsl:element name="title"><xsl:text>Planned features</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="bugs">
	<xsl:element name="sect1">
		<xsl:element name="title"><xsl:text>Known bugs</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<!-- Copy content unchanged -->
<xsl:template match="@*|node()">
	<xsl:copy>
		<xsl:apply-templates select="@*|node()"/>
	</xsl:copy>
</xsl:template>

</xsl:stylesheet>
