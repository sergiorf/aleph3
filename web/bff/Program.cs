using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Nodes;

var builder = WebApplication.CreateBuilder(args);
builder.Services.AddHttpClient("engine", client =>
{
    var baseUrl = builder.Configuration["ALEPH3_ENGINE_BASE_URL"]
        ?? builder.Configuration["Engine:BaseUrl"]
        ?? "http://localhost:8080";
    client.BaseAddress = new Uri(baseUrl);
    client.Timeout = TimeSpan.FromSeconds(30);
});

var app = builder.Build();

const int MaxSourceBytes = 256 * 1024;
var jsonOptions = new JsonSerializerOptions(JsonSerializerDefaults.Web);

app.MapGet("/api/health", () => Results.Json(new
{
    status = "ok",
    service = "aleph3-bff",
    ready = true
}, jsonOptions));

app.MapPost("/api/sessions", async (IHttpClientFactory clientFactory, CancellationToken cancellationToken) =>
{
    var engine = clientFactory.CreateClient("engine");
    try
    {
        using var response = await engine.PostAsync("/internal/sessions", null, cancellationToken);
        var text = await response.Content.ReadAsStringAsync(cancellationToken);
        return PublicEngineResponse(response.StatusCode, text);
    }
    catch (HttpRequestException)
    {
        return PublicError(StatusCodes.Status503ServiceUnavailable, "bff.engine_unavailable", "The internal engine service is unavailable.");
    }
    catch (TaskCanceledException)
    {
        return PublicError(StatusCodes.Status504GatewayTimeout, "bff.engine_timeout", "The internal engine service did not respond in time.");
    }
});

app.MapPost("/api/sessions/{sessionId}/evaluate", async (
    string sessionId,
    HttpRequest request,
    IHttpClientFactory clientFactory,
    CancellationToken cancellationToken) =>
{
    JsonObject body;
    try
    {
        body = await JsonNode.ParseAsync(request.Body, cancellationToken: cancellationToken) as JsonObject
            ?? throw new JsonException("Expected a JSON object.");
    }
    catch (JsonException error)
    {
        return PublicError(StatusCodes.Status400BadRequest, "bff.invalid_json", $"Invalid JSON request body: {error.Message}");
    }

    if (!body.TryGetPropertyValue("source", out var sourceNode) || sourceNode is null || sourceNode.GetValueKind() != JsonValueKind.String)
    {
        return PublicError(StatusCodes.Status400BadRequest, "bff.invalid_request", "Evaluation requests require a string `source` field.");
    }

    var source = sourceNode.GetValue<string>();
    if (System.Text.Encoding.UTF8.GetByteCount(source) > MaxSourceBytes)
    {
        return PublicError(StatusCodes.Status413PayloadTooLarge, "bff.source_too_large", "The expression source exceeds the configured limit.");
    }

    var engine = clientFactory.CreateClient("engine");
    try
    {
        using var content = JsonContent.Create(new { source }, options: jsonOptions);
        using var response = await engine.PostAsync($"/internal/sessions/{Uri.EscapeDataString(sessionId)}/evaluate", content, cancellationToken);
        var text = await response.Content.ReadAsStringAsync(cancellationToken);
        return PublicEngineResponse(response.StatusCode, text);
    }
    catch (HttpRequestException)
    {
        return PublicError(StatusCodes.Status503ServiceUnavailable, "bff.engine_unavailable", "The internal engine service is unavailable.");
    }
    catch (TaskCanceledException)
    {
        return PublicError(StatusCodes.Status504GatewayTimeout, "bff.engine_timeout", "The internal engine service did not respond in time.");
    }
});

app.Run();

static IResult PublicEngineResponse(HttpStatusCode statusCode, string text)
{
    if ((int)statusCode >= 500)
    {
        return PublicError(StatusCodes.Status502BadGateway, "bff.engine_error", "The internal engine service returned an error.");
    }

    try
    {
        var body = JsonNode.Parse(text);
        if (body is null)
        {
            return PublicError(StatusCodes.Status502BadGateway, "bff.invalid_engine_response", "The internal engine service returned an invalid response.");
        }
        return Results.Json(body, statusCode: (int)statusCode);
    }
    catch (JsonException)
    {
        return PublicError(StatusCodes.Status502BadGateway, "bff.invalid_engine_response", "The internal engine service returned an invalid response.");
    }
}

static IResult PublicError(int status, string code, string message)
{
    return Results.Json(new
    {
        status = "error",
        error = new
        {
            code,
            message
        }
    }, statusCode: status);
}
