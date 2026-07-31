FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src
COPY web/bff/Aleph3.Bff.csproj web/bff/
RUN dotnet restore web/bff/Aleph3.Bff.csproj
COPY web/bff web/bff
RUN dotnet publish web/bff/Aleph3.Bff.csproj -c Release -o /app/publish

FROM mcr.microsoft.com/dotnet/aspnet:8.0
WORKDIR /app
RUN apt-get update \
    && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /app/publish .
ENV ASPNETCORE_URLS=http://0.0.0.0:8080
EXPOSE 8080
ENTRYPOINT ["dotnet", "Aleph3.Bff.dll"]
